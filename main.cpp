#include "uvxx/tcp.hpp"
#include <iostream>
#include <unistd.h>
#include "command.hpp"
#include "irc_error.hpp"
#include "irc_server.hpp"
#include "yieldxx/task.hpp"
#include <uv.h>

#define DEFAULT_PORT 6667
#define DEFAULT_BACKLOG 128

using std::string;
using std::vector;
using std::unordered_set;
using std::unordered_map;
using std::unique_ptr;

void cmd_nick(irc_server& irc, user& user, vector<string>& params, unique_ptr<::user>& old_pos);

void cmd_nick(irc_server& irc, user& user, vector<string>& params)
{
	cmd_nick(irc, user, params, irc.users[user.name]);
}

void cmd_nick(irc_server& irc, user& user, vector<string>& params, unique_ptr<::user>& old_pos)
{
	if (params.size() < 1)
		throw irc_error(431, {"No nickname given"});

	auto old_name = user.name;
	auto new_name = params[0];

	for (auto c : new_name)
		if (c == '!' || c == '#' || c == '*' || c == '@')
			throw irc_error(432, {params[0], "Erroneus nickname"});

	auto& new_pos = irc.users[new_name];

	if (new_pos)
		throw irc_error(433, {params[0], "Nickname is already in use"});

	auto notify_msg = make_message(user, "NICK", new_name);

	new_pos = std::move(old_pos);
	user.name = new_name;

	irc.users.erase(old_name);

	auto notify = std::unordered_set<::user*>{&user};
	for (auto r : user.rooms)
		for (auto u : r->users)
			notify.insert(u);

	for (auto n : notify)
		n->send(notify_msg);
}

void cmd_user(irc_server& irc, user& user, vector<string>& params)
{
	if (params.size() < 1)
		throw irc_error(461, {"USER", "Not enough parameters"});

	auto remote_username = params[0];

	for (auto c : remote_username)
		if (c == '!' || c == '#' || c == '*' || c == '@')
			throw irc_error(555, {"Your username is invalid. Please make sure that your username contains only alphanumeric characters."});

	user.remote_username = remote_username;
}

void cmd_join(irc_server& irc, user& user, vector<string>& params)
{
	if (params.size() < 1)
		throw irc_error(461, {"JOIN", "Not enough parameters"});

	auto& room_ptr = irc.rooms[params[0]];
	if (!room_ptr)
		room_ptr = std::make_unique<room>(params[0]);

	auto& room = *room_ptr;
	if (room.users.count(&user))
		return;

	room.users.insert(&user);
	user.rooms.insert(&room);

	room.send(user, "JOIN");

	auto users_list = std::string{};
	for (auto user : room.users)
		users_list += user->name + " ";
	users_list.resize(users_list.size()-1);

	user.send(irc.hostname, "353", {"=", room.name, users_list});
	user.send(irc.hostname, "366", {room.name, "End of /NAMES list."});
}

void cmd_part(irc_server& irc, user& user, vector<string>& params)
{
	if (params.size() < 1)
		throw irc_error(461, {"PART", "Not enough parameters"});

	auto& room_ptr = irc.rooms[params[0]];
	if (!room_ptr)
		throw irc_error(403, {params[0], "No such channel"});

	auto& room = *room_ptr;
	if (!room.users.count(&user))
		throw irc_error(442, {params[0], "You're not on that channel"});

	if (params.size() >= 2)
		room.send(user, "PART", {params[1]});
	else
		room.send(user, "PART");

	room.users.erase(&user);
	user.rooms.erase(&room);
}

void cmd_privmsg(irc_server& irc, user& user, vector<string>& params)
{
	if (params.size() < 1)
		throw irc_error(411, {"No recipient given (PRIVMSG)"});
	else if (params.size() < 2)
		throw irc_error(412, {"No text to send"});

	try
	{
		irc.send(params[0], user.name, "PRIVMSG", {params[1]}, &user);
	}
	catch (std::out_of_range&)
	{
		throw irc_error(401, {params[0], "No such nick/channel"});
	}
}

void cmd_ping(irc_server& irc, user& user, vector<string>& params)
{
	if (params.size() < 1)
		throw irc_error(461, {"PING", "Not enough parameters"});

	user.send(make_message(irc.hostname, "PONG", irc.hostname, {params[0]}));
}

void cmd_who(irc_server& irc, user& user, vector<string>& params)
{
	if (params.size() < 1)
		throw irc_error(461, {"WHO", "Not enough parameters"});

	try
	{
		auto& target_user = irc.users.at(params[0]);
		user.send(irc.hostname, "352", {"*", "~", target_user->remote_hostname, irc.hostname, target_user->name, "H", "0 d"});
	}
	catch (std::out_of_range&)
	{}
	user.send(irc.hostname, "352", {params[0], "End of /WHO list."});
}

void handle_connection(irc_server& irc, uvxx::tcp&& client)
{
	xx::spawn_task([&irc, client = std::move(client)](xx::task&& task) mutable {
		auto cmds = parse_commands(client.read(task)).begin();

		auto user_ptr = std::make_unique<user>("*", std::move(client));
		auto& user = *user_ptr;

		auto quit_message = std::string{};

		user.remote_hostname = user.client.getpeername();

		try
		{
			auto user_done = false;
			auto nick_done = false;

			// At first, we accept only NICK and USER.
			for (; !(user_done && nick_done); ++cmds)
			{
				try
				{
					if (cmds->name == "NICK")
					{
						cmd_nick(irc, user, cmds->params, user_ptr ? user_ptr : irc.users[user.name]);
						nick_done = true;
					}
					else if (cmds->name == "USER")
					{
						cmd_user(irc, user, cmds->params);
						user_done = true;
					}
					else
						throw irc_error{451, {"You have not registered"}};

					if (user_done && nick_done)
						user.send(irc.hostname, "001", {"Welcome " + user.name + "!"});
				}
				catch (irc_error& e)
				{
					user.send(irc.hostname, std::to_string(e.code), e.params);
				}
			}

			// Now we accept all commands.
			for (; cmds->name != "QUIT"; ++cmds)
			{
				try
				{
					if (cmds->name == "JOIN")
						cmd_join(irc, user, cmds->params);

					else if (cmds->name == "PART")
						cmd_part(irc, user, cmds->params);

					else if (cmds->name == "PRIVMSG")
						cmd_privmsg(irc, user, cmds->params);

					else if (cmds->name == "NICK")
						cmd_nick(irc, user, cmds->params);

					else if (cmds->name == "PING")
						cmd_ping(irc, user, cmds->params);

					else if (cmds->name == "WHO")
						cmd_who(irc, user, cmds->params);

					else
						throw irc_error{421, {cmds->name, "Invalid command"}};
				}
				catch (irc_error& e)
				{
					user.send(irc.hostname, std::to_string(e.code), e.params);
				}
			}

			if (cmds->params.size())
				quit_message = cmds->params[0];
		}
		catch (std::out_of_range&)
		{
			// User disconnected.
			// Exception thrown by operator* of the client.read() generator.
		}
		catch (uvxx::error&)
		{
			// Connection lost.
			// Exception thrown by client.read().
		}

		// Remove the user.

		for (auto room : user.rooms)
		{
			room->users.erase(&user);
			room->send(user, "QUIT", quit_message.size() ? vector<string>{quit_message} : vector<string>{});
		}

		irc.users.erase(user.name);
	});
}

int main(int argc, char** argv)
{
	auto host = std::string{"::"};
	auto port = DEFAULT_PORT;

	if (argc >= 2)
		host = argv[1];

	if (argc >= 3)
		port = atoi(argv[2]);

	if (host.find_first_of(":") == std::string::npos && host.find_first_of(":") == std::string::npos)
	{
		port = stoi(host);
		host = "::";
	}

	auto loop = uv_default_loop();
	auto irc = irc_server{};

	{
		char buf[512];
		gethostname(buf, 512);
		irc.hostname = buf;
	}

	auto server = uvxx::tcp{loop};

	std::cout << "Binding to " << host << ", port " << port << "." << std::endl;
	server.bind(host, port);

	xx::spawn_task([=, &irc, &server](xx::task&& task){
		for (auto client_connected : server.listen(task, DEFAULT_BACKLOG))
			handle_connection(irc, server.accept());
	});

	return uv_run(loop, UV_RUN_DEFAULT);
}
