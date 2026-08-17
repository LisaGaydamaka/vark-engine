#include "ui_text_field.h"
#include "ui_renderer.h"
#include "ui_root.h"
#include "core/logger.h"
#include <algorithm>
#include <cctype>
#include <windows.h>

UITextField::UITextField() {
    set_rect(0, 0, 120, 20);
}

void UITextField::set_text(const std::string& text) {
    m_text = text;
    m_cursorPos = (int)m_text.size();
    m_selectionStart = m_selectionEnd = m_cursorPos;
}

// ---- Render (unchanged from original) ----
void UITextField::render(UIRenderer* ui) {
    // Background
    ui->draw_rect(m_rect.x, m_rect.y, m_rect.w, m_rect.h, 0.15f, 0.15f, 0.2f, 1.0f);
    // Border
    ui->draw_rect(m_rect.x, m_rect.y, m_rect.w, 1, 0.3f, 0.3f, 0.4f, 1.0f);
    ui->draw_rect(m_rect.x, m_rect.y + m_rect.h - 1, m_rect.w, 1, 0.3f, 0.3f, 0.4f, 1.0f);
    ui->draw_rect(m_rect.x, m_rect.y, 1, m_rect.h, 0.3f, 0.3f, 0.4f, 1.0f);
    ui->draw_rect(m_rect.x + m_rect.w - 1, m_rect.y, 1, m_rect.h, 0.3f, 0.3f, 0.4f, 1.0f);

    ui->push_clip_rect(m_rect.x + 2, m_rect.y + 2, m_rect.w - 4, m_rect.h - 4);

    float textX = m_rect.x + 4.0f;
    float textY = m_rect.y + (m_rect.h - 8.0f) * 0.5f;

    // Determine display string
    std::string display = m_text;
    if (display.empty() && !m_focused) {
        display = m_placeholder;
    }

    // If focused and there is a selection, draw it
    if (m_focused && m_selectionStart != m_selectionEnd) {
        int selStart = std::min(m_selectionStart, m_selectionEnd);
        int selEnd = std::max(m_selectionStart, m_selectionEnd);
        float selX = textX + selStart * 8.0f;
        float selW = (selEnd - selStart) * 8.0f;
        ui->draw_rect(selX, m_rect.y + 2, selW, m_rect.h - 4, 0.3f, 0.5f, 0.8f, 0.8f);
    }

    // Draw text
    if (!display.empty()) {
        float r = m_focused ? 1.0f : 0.7f;
        float g = m_focused ? 1.0f : 0.7f;
        float b = m_focused ? 1.0f : 0.7f;
        ui->draw_text(textX, textY, display.c_str(), r, g, b, 1.0f);
    }

    // Draw cursor if focused and no selection
    if (m_focused && m_selectionStart == m_selectionEnd) {
        static float time = 0.0f;
        time += 1.0f / 60.0f;
        if (time > 0.5f) {
            m_cursorVisible = !m_cursorVisible;
            time = 0.0f;
        }
        if (m_cursorVisible) {
            float cursorX = textX + m_cursorPos * 8.0f;
            ui->draw_rect(cursorX, m_rect.y + 2, 1.0f, m_rect.h - 4, 1.0f, 1.0f, 1.0f, 1.0f);
        }
    } else {
        m_cursorVisible = true;
    }

    ui->pop_clip_rect();
}

// ---- Step 3: Mouse capture for drag selection ----
bool UITextField::on_mouse_down(float x, float y, int button) {
    if (button != 0) return false;
    request_focus();
    m_dragging = true;

    // Capture the mouse so we get move events even outside the field
    UIRoot* root = get_root();
    if (root) root->set_capture(this, true);

    float relX = x - (m_rect.x + 4.0f);
    int pos = (int)(relX / 8.0f);
    if (pos < 0) pos = 0;
    if (pos > (int)m_text.size()) pos = (int)m_text.size();
    m_cursorPos = pos;
    m_selectionStart = m_selectionEnd = pos;
    return true;
}

bool UITextField::on_mouse_up(float x, float y, int button) {
    if (button != 0) return false;
    m_dragging = false;

    // Release capture
    UIRoot* root = get_root();
    if (root) root->set_capture(this, false);

    return true;
}

bool UITextField::on_mouse_move(float x, float y) {
    if (m_dragging) {
        float relX = x - (m_rect.x + 4.0f);
        int pos = (int)(relX / 8.0f);
        if (pos < 0) pos = 0;
        if (pos > (int)m_text.size()) pos = (int)m_text.size();
        m_cursorPos = pos;
        m_selectionEnd = pos;   // extends selection
        return true;
    }
    return false;
}

// ---- Keyboard input (unchanged) ----
bool UITextField::on_key_down(int key, bool ctrl, bool shift) {
    if (!m_focused) return false;

    // Ctrl+A: select all
    if (ctrl && key == 'A') {
        select_all();
        return true;
    }
    // Ctrl+C: copy
    if (ctrl && key == 'C') {
        copy_to_clipboard();
        return true;
    }
    // Ctrl+X: cut
    if (ctrl && key == 'X') {
        cut_to_clipboard();
        return true;
    }
    // Ctrl+V: paste
    if (ctrl && key == 'V') {
        paste_from_clipboard();
        return true;
    }

    // Enter / Escape
    if (key == VK_RETURN) {
        commit();
        return true;
    }
    if (key == VK_ESCAPE) {
        cancel();
        return true;
    }

    // Backspace
    if (key == VK_BACK) {
        if (m_selectionStart != m_selectionEnd) {
            delete_selection();
        } else if (m_cursorPos > 0) {
            m_text.erase(m_cursorPos - 1, 1);
            m_cursorPos--;
            m_selectionStart = m_selectionEnd = m_cursorPos;
        }
        return true;
    }

    // Delete key
    if (key == VK_DELETE) {
        if (m_selectionStart != m_selectionEnd) {
            delete_selection();
        } else if (m_cursorPos < (int)m_text.size()) {
            m_text.erase(m_cursorPos, 1);
            m_selectionStart = m_selectionEnd = m_cursorPos;
        }
        return true;
    }

    // Arrow keys
    if (key == VK_LEFT) {
        move_cursor(-1, shift);
        return true;
    }
    if (key == VK_RIGHT) {
        move_cursor(1, shift);
        return true;
    }
    if (key == VK_HOME) {
        move_to_home(shift);
        return true;
    }
    if (key == VK_END) {
        move_to_end(shift);
        return true;
    }

    return false;
}

bool UITextField::on_char(char c) {
    if (!m_focused) return false;
    if (std::isprint(static_cast<unsigned char>(c))) {
        if (m_selectionStart != m_selectionEnd) {
            delete_selection();
        }
        insert_char(c);
        return true;
    }
    return false;
}

// ---- Text manipulation (all unchanged) ----
void UITextField::insert_char(char c) {
    m_text.insert(m_cursorPos, 1, c);
    m_cursorPos++;
    m_selectionStart = m_selectionEnd = m_cursorPos;
}

void UITextField::delete_char() {
    if (m_selectionStart != m_selectionEnd) {
        delete_selection();
    } else if (m_cursorPos > 0) {
        m_text.erase(m_cursorPos - 1, 1);
        m_cursorPos--;
        m_selectionStart = m_selectionEnd = m_cursorPos;
    }
}

void UITextField::move_cursor(int delta, bool extendSelection) {
    int newPos = m_cursorPos + delta;
    if (newPos < 0) newPos = 0;
    if (newPos > (int)m_text.size()) newPos = (int)m_text.size();
    m_cursorPos = newPos;
    if (extendSelection) {
        m_selectionEnd = m_cursorPos;
    } else {
        m_selectionStart = m_selectionEnd = m_cursorPos;
    }
}

void UITextField::move_to_home(bool extendSelection) {
    m_cursorPos = 0;
    if (extendSelection) {
        m_selectionEnd = m_cursorPos;
    } else {
        m_selectionStart = m_selectionEnd = m_cursorPos;
    }
}

void UITextField::move_to_end(bool extendSelection) {
    m_cursorPos = (int)m_text.size();
    if (extendSelection) {
        m_selectionEnd = m_cursorPos;
    } else {
        m_selectionStart = m_selectionEnd = m_cursorPos;
    }
}

void UITextField::select_all() {
    m_selectionStart = 0;
    m_selectionEnd = (int)m_text.size();
    m_cursorPos = m_selectionEnd;
}

void UITextField::get_selection(int& start, int& end) const {
    start = std::min(m_selectionStart, m_selectionEnd);
    end = std::max(m_selectionStart, m_selectionEnd);
}

void UITextField::delete_selection() {
    int start, end;
    get_selection(start, end);
    if (start == end) return;
    m_text.erase(start, end - start);
    m_cursorPos = start;
    m_selectionStart = m_selectionEnd = m_cursorPos;
}

void UITextField::replace_selection(const std::string& text) {
    int start, end;
    get_selection(start, end);
    if (start == end) {
        m_text.insert(m_cursorPos, text);
        m_cursorPos += (int)text.size();
    } else {
        m_text.replace(start, end - start, text);
        m_cursorPos = start + (int)text.size();
    }
    m_selectionStart = m_selectionEnd = m_cursorPos;
}

// ---- Clipboard (unchanged) ----
void UITextField::copy_to_clipboard() {
    int start, end;
    get_selection(start, end);
    if (start == end) return;
    std::string selected = m_text.substr(start, end - start);
    if (OpenClipboard(nullptr)) {
        EmptyClipboard();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, selected.size() + 1);
        if (hMem) {
            char* pData = (char*)GlobalLock(hMem);
            memcpy(pData, selected.c_str(), selected.size());
            pData[selected.size()] = '\0';
            GlobalUnlock(hMem);
            SetClipboardData(CF_TEXT, hMem);
        }
        CloseClipboard();
    }
}

void UITextField::cut_to_clipboard() {
    copy_to_clipboard();
    delete_selection();
}

void UITextField::paste_from_clipboard() {
    if (!IsClipboardFormatAvailable(CF_TEXT)) return;
    if (!OpenClipboard(nullptr)) return;
    HANDLE hMem = GetClipboardData(CF_TEXT);
    if (hMem) {
        char* pData = (char*)GlobalLock(hMem);
        if (pData) {
            std::string pasteText(pData);
            GlobalUnlock(hMem);
            if (m_selectionStart != m_selectionEnd) {
                delete_selection();
            }
            m_text.insert(m_cursorPos, pasteText);
            m_cursorPos += (int)pasteText.size();
            m_selectionStart = m_selectionEnd = m_cursorPos;
        }
    }
    CloseClipboard();
}

// ---- Commit / Cancel ----
void UITextField::commit() {
    if (m_onCommit) {
        m_onCommit(m_text);
    }
    set_focus(false);
}

void UITextField::cancel() {
    if (m_onCancel) {
        m_onCancel();
    }
    set_focus(false);
}