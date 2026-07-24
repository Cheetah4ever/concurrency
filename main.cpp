#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include <sys/wait.h>
#include <unistd.h>

enum class Function
{
    words,
    lines,
    characters
};

struct Options
{
    bool use_processes = false;
    bool use_threads = false;
    Function function{};
    bool has_function = false;
    std::string input1;
    std::string input2;
};

struct ChildResult
{
    std::uint64_t count;
    int error;
};

static void usage(const char *program)
{
    std::cerr << "Usage: " << program
              << " (-p | -t) -e <words|lines|characters>"
              << " -i1 <file> -i2 <file>\n";
}

static Function parse_function(const std::string &name)
{
    if (name == "words")
        return Function::words;
    if (name == "lines")
        return Function::lines;
    if (name == "characters")
        return Function::characters;
    throw std::runtime_error(
        "unknown function '" + name +
        "' (choose words, lines, or characters)");
}

static Options parse_options(int argc, char *argv[])
{
    Options options;

    for (int i = 1; i < argc; ++i)
    {
        const std::string argument = argv[i];

        if (argument == "-p")
        {
            options.use_processes = true;
        }
        else if (argument == "-t")
        {
            options.use_threads = true;
        }
        else if (argument == "-e" || argument == "-i1" || argument == "-i2")
        {
            if (++i >= argc)
                throw std::runtime_error(argument + " requires a value");

            if (argument == "-e")
            {
                options.function = parse_function(argv[i]);
                options.has_function = true;
            }
            else if (argument == "-i1")
            {
                options.input1 = argv[i];
            }
            else
            {
                options.input2 = argv[i];
            }
        }
        else
        {
            throw std::runtime_error("unknown option '" + argument + "'");
        }
    }

    if (options.use_processes == options.use_threads)
        throw std::runtime_error("choose exactly one of -p or -t");
    if (!options.has_function)
        throw std::runtime_error("-e is mandatory");
    if (options.input1.empty() || options.input2.empty())
        throw std::runtime_error("-i1 and -i2 are mandatory");

    return options;
}

static std::uint64_t count_file(const std::string &path, Function function)
{
    std::ifstream input(path);
    if (!input)
        throw std::runtime_error(path + ": " + std::strerror(errno));

    std::uint64_t count = 0;

    if (function == Function::words)
    {
        std::string word;
        while (input >> word)
            ++count;
    }
    else if (function == Function::lines)
    {
        std::string line;
        while (std::getline(input, line))
            ++count;
    }
    else
    {
        char character;
        while (input.get(character))
            ++count;
    }

    return count;
}

static std::uint64_t run_threads(const Options &options)
{
    std::uint64_t first = 0;
    std::uint64_t second = 0;
    std::exception_ptr first_error;
    std::exception_ptr second_error;

    std::thread thread1([&] {
        try
        {
            first = count_file(options.input1, options.function);
        }
        catch (...)
        {
            first_error = std::current_exception();
        }
    });

    std::thread thread2([&] {
        try
        {
            second = count_file(options.input2, options.function);
        }
        catch (...)
        {
            second_error = std::current_exception();
        }
    });

    thread1.join();
    thread2.join();

    if (first_error)
        std::rethrow_exception(first_error);
    if (second_error)
        std::rethrow_exception(second_error);

    return first + second;
}

static bool write_all(int fd, const void *buffer, std::size_t size)
{
    const auto *bytes = static_cast<const char *>(buffer);
    while (size > 0)
    {
        const ssize_t written = write(fd, bytes, size);
        if (written < 0)
        {
            if (errno == EINTR)
                continue;
            return false;
        }
        bytes += written;
        size -= static_cast<std::size_t>(written);
    }
    return true;
}

static bool read_all(int fd, void *buffer, std::size_t size)
{
    auto *bytes = static_cast<char *>(buffer);
    while (size > 0)
    {
        const ssize_t received = read(fd, bytes, size);
        if (received == 0)
            return false;
        if (received < 0)
        {
            if (errno == EINTR)
                continue;
            return false;
        }
        bytes += received;
        size -= static_cast<std::size_t>(received);
    }
    return true;
}

static pid_t start_child(const std::string &path, Function function, int pipefd[2])
{
    if (pipe(pipefd) == -1)
        throw std::runtime_error(std::string("pipe: ") + std::strerror(errno));

    const pid_t child = fork();
    if (child == -1)
    {
        const int saved_error = errno;
        close(pipefd[0]);
        close(pipefd[1]);
        throw std::runtime_error(
            std::string("fork: ") + std::strerror(saved_error));
    }

    if (child == 0)
    {
        close(pipefd[0]);
        ChildResult result{0, 0};
        try
        {
            result.count = count_file(path, function);
        }
        catch (...)
        {
            result.error = 1;
        }
        const bool sent = write_all(pipefd[1], &result, sizeof(result));
        close(pipefd[1]);
        _exit(sent ? 0 : 1);
    }

    close(pipefd[1]);
    return child;
}

static ChildResult collect_child(pid_t child, int read_fd)
{
    ChildResult result{};
    const bool received = read_all(read_fd, &result, sizeof(result));
    close(read_fd);

    int status = 0;
    while (waitpid(child, &status, 0) == -1)
    {
        if (errno != EINTR)
            throw std::runtime_error(
                std::string("waitpid: ") + std::strerror(errno));
    }

    if (!received || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
        throw std::runtime_error("child process failed");
    return result;
}

static std::uint64_t run_processes(const Options &options)
{
    int first_pipe[2];
    int second_pipe[2];

    const pid_t first_child =
        start_child(options.input1, options.function, first_pipe);

    pid_t second_child;
    try
    {
        second_child =
            start_child(options.input2, options.function, second_pipe);
    }
    catch (...)
    {
        close(first_pipe[0]);
        waitpid(first_child, nullptr, 0);
        throw;
    }

    const ChildResult first = collect_child(first_child, first_pipe[0]);
    const ChildResult second = collect_child(second_child, second_pipe[0]);

    if (first.error)
        throw std::runtime_error("could not read " + options.input1);
    if (second.error)
        throw std::runtime_error("could not read " + options.input2);

    return first.count + second.count;
}

static const char *function_name(Function function)
{
    switch (function)
    {
    case Function::words:
        return "words";
    case Function::lines:
        return "lines";
    case Function::characters:
        return "characters";
    }
    return "items";
}

int main(int argc, char *argv[])
{
    try
    {
        const Options options = parse_options(argc, argv);
        const std::uint64_t total = options.use_threads
                                        ? run_threads(options)
                                        : run_processes(options);

        std::cout << total << " total "
                  << function_name(options.function) << '\n';
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "error: " << error.what() << '\n';
        usage(argv[0]);
        return 1;
    }
}
