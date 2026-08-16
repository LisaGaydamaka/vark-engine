#include "csg_solver.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>

void print_usage() {
    printf("Vibe Engine CSG Rebake Tool\n");
    printf("Usage:\n");
    printf("  vcompiler <input.vmis>\n");
    printf("  Reads the .vmis file, re-runs CSG on its brushes, and overwrites the baked mesh.\n");
    printf("  Also exports a .obj file for debugging.\n");
    printf("\nInteractive mode (type a path to a .vmis file):\n");
    printf("  vcompiler (then type path when prompted)\n");
    printf("  Type 'exit' or 'quit' to close.\n");
}

bool rebake_file(const std::string& vmisPath) {
    printf("Input:  %s\n", vmisPath.c_str());
    if (!rebake_vmis(vmisPath.c_str())) {
        printf("Rebake failed.\n");
        return false;
    }
    printf("Rebake successful.\n");
    return true;
}

int main(int argc, char** argv) {
    printf("=== Vibe Engine CSG Rebake Tool ===\n");

    if (argc >= 2) {
        const char* vmisPath = argv[1];
        if (rebake_file(vmisPath)) {
            return 0;
        } else {
            return 1;
        }
    }

    print_usage();
    printf("\nEnter the path to a .vmis file (or 'exit' to quit):\n");

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

        rebake_file(line);
        printf("\nReady for next file.\n");
    }

    return 0;
}