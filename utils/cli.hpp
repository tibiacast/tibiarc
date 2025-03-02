/*
 * Copyright 2025 "John Högberg"
 *
 * This file is part of tibiarc.
 *
 * tibiarc is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * tibiarc is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with tibiarc. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef __TRC_CLI_HPP__
#define __TRC_CLI_HPP__

#include <algorithm>
#include <exception>
#include <functional>
#include <iostream>
#include <ranges>
#include <string>

namespace trc {
namespace CLI {
using Range = std::ranges::subrange<std::vector<std::string>::const_iterator>;

struct Parameter {
    std::string Description;
    std::vector<std::string> Arguments;
    std::function<void(const CLI::Range &)> Parse;
};

static void Error(const std::string &program_path, const std::string &slug) {
    std::cerr << program_path << ": " << slug << std::endl;
    std::cerr << "Try `" << program_path << "--help` or `" << program_path
              << "--usage` for more information." << std::endl;
    std::exit(1);
}

static void Help(
        const std::string &program_description,
        const std::vector<std::pair<std::string, Parameter>> &named_parameters,
        const std::vector<std::string> &positional_parameters,
        const std::string &program_path) {
    std::cout << "Usage: " << program_path;

    if (named_parameters.size() > 0) {
        std::cout << " [OPTION...]";
    }

    for (const auto &name : positional_parameters) {
        std::cout << " " << name;
    }

    std::cout << "\n" << program_description << "\n\n";

    for (const auto &[name, parameter] : named_parameters) {
        std::cout << "\t--" << name;

        for (const auto &argument : parameter.Arguments) {
            std::cout << " " << argument;
        }

        std::cout << "\n\t\t" << parameter.Description << std::endl;
    }

    std::exit(0);
}

static void Usage(
        const std::string &program_path,
        const std::vector<std::pair<std::string, Parameter>> &named_parameters,
        const std::vector<std::string> &positional_parameters) {
    std::cout << "Usage: " << program_path << " ";

    for (const auto &[name, parameter] : named_parameters) {
        std::cout << " [--" << name;

        for (const auto &argument : parameter.Arguments) {
            std::cout << " " << argument;
        }

        std::cout << "]";
    }

    for (const auto &name : positional_parameters) {
        std::cout << " " << name;
    }

    std::cout << std::endl;

    std::exit(0);
}

static void Version(const std::string &version) {
    std::cout << version << std::endl;
    std::exit(0);
}

static void UnrecognizedOption(const std::string &name,
                               const std::string &option) {
    Error(name, "unrecognized option '" + option + "'");
}

std::vector<std::string> Process(
        int argc,
        char **argv,
        const std::string &description,
        const std::string &version,
        const std::vector<std::string> &positional_parameters,
        std::vector<std::pair<std::string, Parameter>> &&named_parameters) {
    if (argc == 0) {
        std::terminate();
    }

    std::vector<std::string> arguments(&argv[1], &argv[argc]);
    std::vector<std::string> positional;
    std::string path = argv[0];

    (void)named_parameters.emplace_back(
            "help",
            Parameter{"Give this help list",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &) {
                          Help(description,
                               named_parameters,
                               positional_parameters,
                               path);
                      }});
    (void)named_parameters.emplace_back(
            "usage",
            Parameter{"Give a short usage message",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &) {
                          Usage(path, named_parameters, positional_parameters);
                      }});
    (void)named_parameters.emplace_back(
            "version",
            Parameter{"Print program version",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &) {
                          Version(version);
                      }});

    for (auto it = arguments.cbegin(); it != arguments.cend();) {
        const auto &argument = *it;
        it = std::next(it);

        if (argument == "--") {
            positional.insert(positional.end(), it, arguments.cend());
            break;
        }

        if (argument.starts_with("--")) {
            auto option = argument.substr(2);
            auto parameter = std::find_if(named_parameters.cbegin(),
                                          named_parameters.cend(),
                                          [&option](const auto &pair) {
                                              return pair.first == option;
                                          });

            if (parameter == named_parameters.cend()) {
                UnrecognizedOption(path, option);
            }

            auto required = parameter->second.Arguments.size();
            if (std::distance(it, arguments.cend()) < required) {
                Error(path, "not enough arguments for --" + argument);
            }

            try {
                auto start = it;

                std::advance(it, required);

                parameter->second.Parse(CLI::Range(start, it));
            } catch (const char *message) {
                Error(path, message);
            }
        } else {
            positional.push_back(argument);
        }
    }

    if (positional.size() != positional_parameters.size()) {
        Usage(path, named_parameters, positional_parameters);
    }

    return positional;
}
} // namespace CLI
} // namespace trc
#endif
