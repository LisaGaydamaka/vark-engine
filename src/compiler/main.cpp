#include "csg_solver.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>

void print_usage()
{
    printf("Vibe Engine CSG Compiler\n");
    printf("Usage:\n");
    printf("  vcompiler <input.vm> [output_base_name]\n");
    printf("  If output base name is omitted, the .vm's basename is used.\n");
    printf("  This will produce a .vmis file (the single-file level container).\n");
    printf("\nInteractive mode (type a path to a .vm file):\n");
    printf("  vcompiler (then type path when prompted)\n");
    printf("  Type 'exit' or 'quit' to close.\n");
}

bool compile_file(const std::string& vmPath, const std::string& baseName)
{
    std::string vmisPath = baseName + ".vmis";
    printf("Input:  %s\n", vmPath.c_str());
    printf("Output: %s (VMIS)\n", vmisPath.c_str());

    if (!compile_vm_to_vmis(vmPath.c_str(), vmisPath.c_str())) {
        printf("VMIS compilation failed.\n");
        return false;
    }

    printf("Compilation successful.\n");
    return true;
}

int main(int argc, char** argv)
{
    printf("=== Vibe Engine CSG Compiler ===\n");

    // ----- Command-line mode -----
    if (argc >= 2) {
        const char* vmPath = argv[1];
        std::string baseName;

        if (argc >= 3) {
            baseName = argv[2];
        } else {
            // Auto-generate base name: strip extension
            baseName = vmPath;
            size_t dot = baseName.rfind('.');
            if (dot != std::string::npos && dot > 0) {
                baseName = baseName.substr(0, dot);
            }
        }

        if (compile_file(vmPath, baseName)) {
            return 0;
        } else {
            return 1;
        }
    }

    // ----- Interactive mode -----
    print_usage();
    printf("\nEnter the path to a .vm file (or 'exit' to quit):\n");

    std::string line;
    while (true) {
        printf("> ");
        std::getline(std::cin, line);

        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);

        if (line == "exit" || line == "quit") {
            printf("Goodbye.\n");
            break;
        }

        // Derive base name from the input path
        std::string baseName = line;
        size_t dot = baseName.rfind('.');
        if (dot != std::string::npos && dot > 0) {
            baseName = baseName.substr(0, dot);
        } else {
            baseName += ".out";  // fallback
        }

        compile_file(line, baseName);
        printf("\nReady for next file.\n");
    }

    return 0;
}