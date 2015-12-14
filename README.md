# ircxxd
Simple IRC server using uvxx

## Building
```sh
% git submodule update --init
% mkdir build
% cd build
% cmake ..
% make
```

## Running
```sh
% ./ircxxd_boost [host] [port]
	or
% ./ircxxd_thread [host] [port]
```

Both host and port are optional (and you may skip the host, set only the port).

The default host is "::" (bind everywhere).

The default port is 6667.

## Requirements

* Fully C++14-compliant compiler. Tested on gcc 5.2 and clang 3.7.
* libuv
* Boost backend: Boost.Context 1.58+

## License - GNU AGPLv3+
Copyright (C) 2015 Janusz Lewandowski

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as
published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
