/*
 * Copyright (C) 2013 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Marcus Tomlinson <marcus.tomlinson@canonical.com>
 */

#pragma once

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <unity/UnityExceptions.h>

namespace unity
{

namespace test
{

namespace scopes
{

namespace internal
{

namespace smartscopes
{

class RaiiServer
{
public:
    RaiiServer(std::string const& server_path, std::string const& arg = "")
    {
        int pipefd[2];
        if (pipe(pipefd) < 0)
        {
            throw unity::ResourceException("Pipe creation failed");
        }

        switch (pid_ = fork())
        {
            case -1:
                throw unity::ResourceException("Failed to fork process");
            case 0: // child
                close(STDOUT_FILENO);   // close stdout
                close(pipefd[0]);       // close read
                if (dup(pipefd[1]) < 0) // open write
                {
                    throw unity::ResourceException("Write pipe duplication failed");
                }

                // execle() instead of execl(). Otherwise, valgrind complains about
                // an invalid envp being passed from execl() to execle().
                execle(server_path.c_str(), server_path.c_str(), arg.c_str(), nullptr, nullptr);
                throw unity::ResourceException("Failed to execute fake server script");
            default: // parent
                close(pipefd[1]);       // close write

                char port_str[10];
                ssize_t bytes_read = read(pipefd[0], port_str, sizeof(port_str) - 1);
                if(bytes_read < 0)
                {
                    throw unity::ResourceException("Failed to read from pipe");
                }
                port_str[bytes_read] = '\0';

                port_ = std::stoi(port_str);
        }
    }

    ~RaiiServer()
    {
        kill(pid_, SIGABRT);
        int status;
        waitpid(pid_, &status, 0);
    }

    pid_t pid_ = -1;
    int port_ = 0;
};

} // namespace smartscopes

} // namespace internal

} // namespace scopes

} // namespace test

} // namespace unity
