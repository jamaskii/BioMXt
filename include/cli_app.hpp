#pragma once
#include <string>
#include <vector>
#include <stdexcept>
#include <iostream>


namespace cliapp {
    class Result {
        public:
            bool success = false;
            std::string value;
    };
    class Option {
        friend class App;
        friend class Command;
        protected:
            bool provided = false;
            std::string value;
        public:
            const std::string name;
            const std::string short_name;
            const std::string description;
            const std::string default_value;
            const bool required;
            const bool has_value;
            static Option option_with_value(std::string name, std::string short_name, std::string description, std::string default_value) {
                return Option(name, short_name, description, default_value, false, true);
            }

            static Option option_required(std::string name, std::string short_name, std::string description) {
                return Option(name, short_name, description, "", true, true);
            }

            static Option option_without_value(std::string name, std::string short_name, std::string description) {
                return Option(name, short_name, description, "", false, false);
            }
            std::string get_value() { return value; }
            bool is_provided() { return provided; }
        private:
            Option(std::string name, std::string short_name, std::string description, std::string default_value, bool required, bool has_value)
                : name(name), short_name(short_name), description(description), default_value(default_value), required(required), has_value(has_value) {}
    };

    class Argument {
        friend class App;
        friend class Command;
        protected:
            bool provided = false;
            std::string value;
        public:
            const std::string name;
            const std::string description;
            Argument(std::string name, std::string description)
                : name(name), description(description) {}
            std::string get_value() { return value; }
            bool is_provided() { return provided; }
    };

    class Command {
        friend class App;
        protected:
            bool provided = false;
            std::vector<Argument> arguments;
            std::vector<Option> options;
        public:
            const std::string name;
            const std::string description;
            Command(std::string name, std::string description)
                : name(name), description(description) {}
            Command &add_argument(Argument argument) { this->arguments.push_back(argument); return *this; }
            Command &add_option(Option option) { this->options.push_back(option); return *this; }
            Argument& find_argument(std::string name) {
                for (Argument& arg : this->arguments) {
                    if (arg.name == name) {
                        return arg;
                    }
                }
                throw std::runtime_error("Argument not found");
            }

            Option& find_option(std::string name, std::string short_name) {
                for (Option& opt : this->options) {
                    if (opt.name == name || opt.short_name == short_name) {
                        return opt;
                    }
                }
                throw std::runtime_error("Option not found");
            }

            void print_help(std::string app_name) {
                std::cout << "Usage: " << app_name << " " << name;
                for (Argument arg : arguments) {
                    std::cout << " <" << arg.name << ">";
                }
                std::cout << " [Options]" << std::endl;
                
                std::cout << "Options:" << std::endl;
                for (Option opt : options) {
                    std::cout << "  " << opt.name << ", " << opt.short_name << "\t" << opt.description;
                    if (opt.required) {
                        std::cout << " (required)";
                    }
                    if (!opt.has_value) {
                        std::cout << " (value not required)";
                    }
                    std::cout << std::endl;
                }
            }

            bool is_provided() { return provided; }

    };


    class App {
        private:
            bool parsed = false;
            std::vector<Command*> commands;
            std::vector<Option> options;
            std::string executable_name;
        public:
            const std::string name;
            const std::string version;
            const std::string description;
            
            App(std::string name, std::string version, std::string description)
                : name(name), version(version), description(description) {}
            App& add_option(Option option) { this->options.push_back(option); return *this; }
            App& add_command(Command* command) { this->commands.push_back(command); return *this; }
            bool parse(int argc, char* argv[]) {
                
                // Clear the provided flag of commands and options
                for (Command* command : this->commands) {
                    command->provided = false;
                }
                for (Option& option : this->options) {
                    option.provided = false;
                }

                // The first argv is the executable name
                executable_name = argv[0];

                // If argc is 1, means no command or option provided
                if (argc == 1) {
                    print_help();
                    return false;
                }

                // The second argv is the command name
                Command* user_command = nullptr;
                for (Command* command : this->commands) {
                    if (command->name == argv[1]) {
                        user_command = command;
                        user_command->provided = true;
                        break;
                    }
                }

                // No command matched
                if (user_command == nullptr) {
                    // argv[1] is not an option, means unknown command provided
                    if (argv[1][0] != '-') {
                        std::cerr << "Error: Unknown command: " << argv[1] << std::endl;
                        print_help();
                        return false;
                    }

                    // Scan thoes argv left, skip the first one which is the executable name
                    for (size_t i = 1; i < argc; i++) {
                        // Try to match option name or short name
                        bool argv_vaild = false;
                        for (Option& option : this->options) {
                            if (option.name == argv[i] || option.short_name == argv[i]) {
                                if (option.has_value) {
                                    // Current option requires a value, but the next argv is an option or the end of argv
                                    if (i + 1 >= argc || argv[i + 1][0] == '-') {
                                        std::cerr << "Error: Option " << argv[i] << " requires a value" << std::endl;
                                        print_help();
                                        return false;
                                    }
                                    option.value = argv[i + 1];
                                    i++;
                                }
                                option.provided = true;
                                argv_vaild = true;
                                break;
                            }
                        }
                        if (!argv_vaild) {
                            std::cerr << "Warning: Unknown option: " << argv[i] << std::endl;
                            if (i + 1 < argc && argv[i + 1][0] != '-') {
                                i++;
                            }
                        }
                    }

                    // Scan complete, check if required options are provided, and at least one option is provided since now is no command mode.
                    bool at_least_one_option_provided = false;
                    for (Option& option : this->options) {
                        if (!option.provided) {
                            if (option.required) {
                                std::cerr << "Error: Option " << option.name << " is required" << std::endl;
                                print_help();
                                return false;
                            }
                        } else {
                            at_least_one_option_provided = true;
                        }
                    }

                    if (!at_least_one_option_provided) {
                        print_help();
                        return false;
                    }

                    return true;
                }

                // Command matched, then parse its arguments and options
                else if (user_command != nullptr) {
                    // Scan thoes argv left, skip the first two argv which are the executable name and command.
                    for (size_t i = 2; i < argc; i++) {
                        // Option
                        if (argv[i][0] == '-') {
                            for (Option& option : user_command->options) {
                                if (option.name == argv[i] || option.short_name == argv[i]) {
                                    option.provided = true;
                                    if (option.has_value) {
                                        // Current option requires a value, but the next argv is an option or the end of argv
                                        if (i + 1 >= argc || argv[i + 1][0] == '-') {
                                            std::cerr << "Error: Option " << argv[i] << " requires a value" << std::endl;
                                            user_command->print_help(name);
                                            return false;
                                        }
                                        option.value = argv[i + 1];
                                        i++;
                                    }
                                    break;
                                }
                            }
                        // Argument
                        } else {
                            bool argv_used = false;
                            for (Argument& argument : user_command->arguments) {
                                if (!argument.provided) {
                                    argument.value = argv[i];
                                    argument.provided = true;
                                    argv_used = true;
                                    break;
                                }
                            }

                            if (!argv_used) {
                                std::cerr << "Warning: Unused argument: \"" << argv[i] << "\"" << std::endl;
                            }
                        }
                    }

                    // Check if help option is provided
                    for (const Option& option : user_command->options) {
                        if (option.provided && option.name == "--help") {
                            return true;
                        }
                    }

                    // Scan complete, check if required arguments and options are provided
                    for (const Argument& argument : user_command->arguments) {
                        if (!argument.provided) {
                            std::cerr << "Error: Argument <" << argument.name << "> is required" << std::endl;
                            user_command->print_help(name);
                            return false;
                        }
                    }
                    for (const Option& option : user_command->options) {
                        if (option.required && !option.provided) {
                            std::cerr << "Error: Option [" << option.name << "] is required" << std::endl;
                            user_command->print_help(name);
                            return false;
                        }
                    }
                    
                }

                return true;
            }

            void print_help() {
                std::cout << "Usage: " << name << " <command> [arguments] [options]\n\n";
                std::cout << "Commands:\n";
                for (const Command* cmd : commands) {
                    std::cout << "  " << cmd->name << "\t" << cmd->description << std::endl;
                }
                std::cout << "\nOptions:\n";
                for (const Option& opt : options) {
                    std::cout << "  " << opt.name << ", " << opt.short_name << "\t" << opt.description << std::endl;
                }
            }

            Option& find_option(std::string name, std::string short_name) {
                for (Option& opt : this->options) {
                    if (opt.name == name || opt.short_name == short_name) {
                        return opt;
                    }
                }
                throw std::runtime_error("Option not found");
            }

    };

    
    
}
