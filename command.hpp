#pragma once

#include <string>
#include <vector>

struct command
{
	std::string name;
	std::vector<std::string> params;
};

template <class T>
auto parse_commands(T iterable) -> xx::generated<command>
{
	return xx::generated<command>([=](xx::generator<command>::yield&& yield)
	{
		auto buffer = std::string{};
		for (auto recv : iterable)
		{
			buffer += recv;

			auto nl = 0;
			while ((nl = buffer.find_first_of('\n')) != std::string::npos)
			{
				auto line = buffer.substr(0, nl);
				buffer = buffer.substr(nl+1);

				auto cmd = command{};
				auto sp = line.find_first_of(' ');
				if (sp == std::string::npos)
				{
					cmd.name = std::move(line);
					line = {};
				}
				else
				{
					cmd.name = line.substr(0, sp);
					line = line.substr(sp+1);
				}

				while (line.size() && (line[0] != ':') && (sp = line.find_first_of(' ')) != std::string::npos)
				{
					cmd.params.emplace_back(line.substr(0, sp));
					line = line.substr(sp+1);
				}

				if (line.size())
				{
					if (line[0] == ':')
						cmd.params.emplace_back(line.substr(1));
					else
						cmd.params.emplace_back(std::move(line));
				}

				yield(cmd);
			}
		}
	});
}
