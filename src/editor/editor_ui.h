#pragma once
#include <vector>
#include <string>

class Editor;

enum class EditField {
    None,
    PosX, PosY, PosZ,
    SizeX, SizeY, SizeZ
};

void BuildEditorUI(Editor& editor);