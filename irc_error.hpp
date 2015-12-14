#pragma once

struct irc_error: public std::exception
{
	int code;
	std::vector<std::string> params;

	irc_error(int code, std::vector<std::string>&& params): code{code}, params{std::move(params)} {}
};
