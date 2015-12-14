#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

struct user;
struct room;

struct user
{
	std::string name;
	std::unordered_set<room*> rooms;
	uvxx::tcp client;

	std::string remote_username;
	std::string remote_hostname;

	user(std::string name): name(std::move(name)) {}
	user(std::string name, uvxx::tcp client): name(std::move(name)), client(std::move(client)) {}

	void send(const std::string& message);
	void send(const std::string& sender, const std::string& command, const std::vector<std::string>& params = {});
	void send(const user& sender, const std::string& command, const std::vector<std::string>& params = {});
};

struct room
{
	std::string name;
	std::unordered_set<user*> users;

	room(std::string name): name(std::move(name)) {}

	void send(const std::string& message, user* skip_user = nullptr);
	void send(const std::string& sender, const std::string& command, const std::vector<std::string>& params = {});
	void send(const user& sender, const std::string& command, const std::vector<std::string>& params = {});
};

struct irc_server
{
	std::string hostname;

	std::unordered_map<std::string, std::unique_ptr<user>> users;
	std::unordered_map<std::string, std::unique_ptr<room>> rooms;

	void send(const std::string& target, const std::string& sender, const std::string& command, const std::vector<std::string>& params = {}, user* skip_user = nullptr);
	void send(const std::string& target, const user& sender, const std::string& command, const std::vector<std::string>& params = {}, user* skip_user = nullptr);
};

inline auto make_message(const std::string& sender, const std::string& command, const std::string& target, const std::vector<std::string>& params = {})
{
	auto msg = ":" + sender + " " + command + " " + target;

	if (params.size())
	{
		auto i = 0;
		for (; i < params.size() - 1; ++i)
			msg += " " + params[i];

		msg += " :" + params[i];
	}

	return msg + "\n";
}

inline auto make_message(const user& sender, const std::string& command, const std::string& target, const std::vector<std::string>& params = {})
{
	return make_message(sender.name + "!" + sender.remote_username + "@" + sender.remote_hostname, command, target, params);
}

inline void user::send(const std::string& message)
{
	client.write(message);
}

inline void user::send(const std::string& sender, const std::string& command, const std::vector<std::string>& params)
{
	send(make_message(sender, command, name, params));
}

inline void user::send(const user& sender, const std::string& command, const std::vector<std::string>& params)
{
	send(make_message(sender, command, name, params));
}

inline void room::send(const std::string& message, user* skip_user)
{
	for (auto target_user : users)
	{
		if (target_user == skip_user)
			continue;

		target_user->send(message);
	}
}

inline void room::send(const std::string& sender, const std::string& command, const std::vector<std::string>& params)
{
	send(make_message(sender, command, name, params));
}

inline void room::send(const user& sender, const std::string& command, const std::vector<std::string>& params)
{
	send(make_message(sender, command, name, params));
}

// throws std::out_of_range if target isn't found
inline void irc_server::send(const std::string& target, const std::string& sender, const std::string& command, const std::vector<std::string>& params, user* skip_user)
{
	auto message = make_message(sender, command, target, params);

	if (target[0] != '#')
	{
		users.at(target)->send(message);
		return;
	}

	rooms.at(target)->send(message, skip_user);
}

inline void irc_server::send(const std::string& target, const user& sender, const std::string& command, const std::vector<std::string>& params, user* skip_user)
{
	auto message = make_message(sender, command, target, params);

	if (target[0] != '#')
	{
		users.at(target)->send(message);
		return;
	}

	rooms.at(target)->send(message, skip_user);
}
