#include <iostream>
#include <filesystem>
#include <vector>
#include <fstream>
#include <string>

// Alias
using sv = std::string_view;

// Version
constexpr int VERSION_MAJOR = 0;
constexpr int VERSION_MINOR = 6;
constexpr int VERSION_PATCH = 10;

// Argc
constexpr int MIN_ARGC = 2;

enum Flags
{
    SKIP_CONFIRMATION = 1 << 0,
    CONFIRM_EACH = 1 << 1,
    FROM_FILE = 1 << 2,
    FROM_DIR = 1 << 4,
};

// Print help and exit
void help()
{
    std::cout << R"(xreplace - batch file content replacer
Usage:
  xreplace [flags] --file <source_file> <destination_directory> <extension>
  xreplace [flags] --dir  <source_directory> <destination_directory> <extension>

Arguments:
  -f, --file <path>   Use a single file as the replacement source.
  -d, --dir  <path>   Use all files  with the fitting extension in a directory
                      as sources. Files will beassigned to targets in a fair, 
                      even split.

  <destination_directory>
                      Path to the folder containing files to be overwritten.

  <extension>         Extension (with dot) of files to replace and read from.
                      Example: .obj

Flags:
  -y, --yes           Skip the initial confirmation.
  -a, --ask           Ask before overwriting each target file.
  -h, --help          Show this help text and exit.
  -v, --version       Show program version and exit.

Behavior:
  - In --file mode: the same source file is copied into every matching target.
  - In --dir mode: target files are distributed evenly among the source files.
    Example: 3 sources, 200 targets to 67, 67, and 66 targets each.
  - Only files with the specified extension are replaced or read.

WARNING:
  This program overwrites files permanently. There is no undo.
)" << std::endl;

    exit(0);
}

// Print version and exit
void version()
{
    std::cout << "xreplace is running version " << VERSION_MAJOR << "." << VERSION_MINOR << "." << VERSION_PATCH << std::endl;
    exit(0);
}

// Check if arguments are sufficient and process them
void handle_arguments(int argc, char **argv, std::string &source, std::string &dest_dir, std::string &extension, uint64_t &flags)
{
    // Check argument count
    if (argc < MIN_ARGC)
    {
        throw std::runtime_error("Unexpected argument count");
    }

    int beginning_position = 1;

    // Process arguments
    for (int i = 1; i < argc; i++)
    {
        std::string_view arg = argv[i];

        if (arg == "-h" || arg == "--help")
        {
            help();
        }
        else if (arg == "-v" || arg == "--version")
        {
            version();
        }
        else if (arg == "-y" || arg == "--yes")
        {
            flags |= Flags::SKIP_CONFIRMATION;
            beginning_position++;
        }
        else if (arg == "-a" || arg == "--ask")
        {
            flags |= Flags::CONFIRM_EACH;
            beginning_position++;
        }
        else if (arg == "-d" || arg == "--dir")
        {
            if (i == argc - 1 || argv[i + 1][0] == '-')
                throw std::runtime_error("--dir requires path");
            source = argv[i + 1];
            flags |= Flags::FROM_DIR;
            beginning_position += 2;
            i++;
        }
        else if (arg == "-f" || arg == "--file")
        {
            if (i == argc - 1 || argv[i + 1][0] == '-')
                throw std::runtime_error("--file requires file");
            source = argv[i + 1];
            flags |= Flags::FROM_FILE;
            beginning_position += 2;
            i++;
        }
        else if (arg.at(0) == '-')
        {
            throw std::runtime_error("Unknown argument: " + std::string(arg));
        }
    }

    if (argc != beginning_position + 2)
    {
        throw std::runtime_error("Unfulfilled arguments");
    }

    dest_dir = argv[beginning_position];
    extension = argv[beginning_position + 1];
}

// Self explanatory
void copy_file_contents(sv src_file, sv dest_file)
{
    // Open source file
    std::ifstream src(std::string(src_file), std::ios::binary);
    if (!src)
    {
        throw std::runtime_error("Failed to open source file: " + std::string(src_file));
    }

    // Open destination file
    std::ofstream dst(std::string(dest_file), std::ios::binary);
    if (!dst)
    {
        throw std::runtime_error("Failed to open destination file: " + std::string(dest_file));
    }

    // Copy all contents
    dst << src.rdbuf();
}

// Prompt user for confirmation
void confirm_overwrite()
{
    std::cout << "Continue? (y/n): ";

    std::string response;
    std::getline(std::cin, response);

    if (response.empty() || (response.at(0) != 'y' && response.at(0) != 'Y'))
    {
        exit(1);
    }
}

void perform_write_file(sv src_file, sv dest_dir, sv extension, uint64_t &overwritten_files, uint64_t &flags)
{
    // Collect destination files with matching extension
    std::vector<std::filesystem::path> dest_files;
    for (const auto &entry : std::filesystem::directory_iterator(dest_dir))
    {
        if (entry.is_regular_file() && entry.path().extension() == extension)
        {
            dest_files.push_back(entry.path());
        }
    }

    // If no destination files were found
    if (dest_files.empty())
    {
        throw std::runtime_error("No destination files found with the given extension");
    }

    // Overwrite each destination file
    for (const auto &dest_path : dest_files)
    {
        if (flags & Flags::CONFIRM_EACH)
        {
            std::cout << "Target: " << dest_path.filename() << "\n";
            confirm_overwrite();
        }

        copy_file_contents(std::string(src_file), std::string(std::filesystem::absolute(dest_path)));
        overwritten_files++;
    }
}

void perform_write_dir(sv source, sv dest_dir, sv extension, uint64_t &overwritten_files, uint64_t &flags)
{
    // Collect source files with matching extension
    std::vector<std::filesystem::path> src_files;
    for (auto &p : std::filesystem::directory_iterator(source))
    {
        if (p.is_regular_file() && p.path().extension() == extension)
        {
            src_files.push_back(p.path());
        }
    }

    // Collect destination files with matching extension
    std::vector<std::filesystem::path> dest_files;
    for (auto &p : std::filesystem::directory_iterator(dest_dir))
    {
        if (p.is_regular_file() && p.path().extension() == extension)
        {
            dest_files.push_back(p.path());
        }
    }

    if (src_files.empty())
    {
        throw std::runtime_error("No source files found with the given extension");
    }
    if (dest_files.empty())
    {
        throw std::runtime_error("No destination files found with the given extension");
    }

    // Distribute destination files evenly among source files
    size_t src_count = src_files.size();
    size_t dest_count = dest_files.size();

    size_t base_count = dest_count / src_count; // minimum files per source
    size_t remainder = dest_count % src_count;  // extra files for the first few sources

    auto dest_it = dest_files.begin();
    for (size_t i = 0; i < src_count; ++i)
    {
        size_t count_for_this_src = base_count + (i < remainder ? 1 : 0);
        for (size_t j = 0; j < count_for_this_src && dest_it != dest_files.end(); ++j, ++dest_it)
        {
            if (flags & Flags::CONFIRM_EACH)
            {
                std::cout << "Target: " << dest_it->filename() << "\n";
                confirm_overwrite();
            }
            copy_file_contents(std::string(src_files[i]), std::string(std::filesystem::absolute(*dest_it)));
            overwritten_files++;
        }
    }
}

void validate_arguments(sv source, sv dest_dir, sv extension, const uint64_t flags)
{
    // Check if any required arguments are empty
    if (source.empty() || dest_dir.empty() || extension.empty())
    {
        throw std::runtime_error("Critical argument is unfulfilled");
    }

    // Verify that only one option is set
    if ((flags & Flags::FROM_FILE) && (flags & Flags::FROM_DIR))
    {
        throw std::runtime_error("Cannot specify both --file and --dir");
    }

    // Verify that source is valid (as directory)
    if (flags & Flags::FROM_DIR)
    {
        if (!(std::filesystem::exists(source) && std::filesystem::is_directory(source)))
        {
            throw std::runtime_error("Directory is invalid: " + std::string(source));
        }
    }

    // Verify that source is valid (as file)
    if (flags & Flags::FROM_FILE)
    {
        if (!(std::filesystem::exists(source) && std::filesystem::is_regular_file(source)))
        {
            throw std::runtime_error("File is invalid: " + std::string(source));
        }
    }

    // Verify that dest_dir is valid
    if (!(std::filesystem::exists(dest_dir) && std::filesystem::is_directory(dest_dir)))
    {
        throw std::runtime_error("Directory is invalid: " + std::string(dest_dir));
    }

    // Verify that extension is valid
    if (extension.at(0) != '.')
    {
        throw std::runtime_error("Extensions should start with a dot. Example: .txt");
    }
}

int main(int argc, char **argv)
{
    std::string source;
    std::string dest_dir;
    std::string extension;
    uint64_t flags = 0;
    uint64_t overwritten_files = 0;

    try
    {
        // Set up arguments
        handle_arguments(argc, argv, source, dest_dir, extension, flags);
        validate_arguments(source, dest_dir, extension, flags);

        // Ask the user to continue
        if (!(flags & Flags::SKIP_CONFIRMATION))
        {
            std::cout << "Target directory: " << dest_dir << "\n";
            confirm_overwrite();
        }

        // Perform the write
        if (flags & Flags::FROM_FILE)
        {
            perform_write_file(source, dest_dir, extension, overwritten_files, flags);
        }
        else if (flags & Flags::FROM_DIR)
        {
            perform_write_dir(source, dest_dir, extension, overwritten_files, flags);
        }
        else
        {
            throw std::runtime_error("Invalid argument");
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "ERROR: " + std::string(e.what()) + "\n";
        std::cerr << "INFO: Try --help" << std::endl;
        return 1;
    }

    std::cout << "INFO: Overwritten files: " << overwritten_files << std::endl;
    return 0;
}
