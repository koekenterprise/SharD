#ifndef SHARD_HPP
#define SHARD_HPP

#include <string>
#include <vector>
#include <ncurses.h>

struct Coords {
    int x = -1;
    int y = -1;
};

struct Selection {
    Coords start;
    Coords end;
};

class Shard {
public:
    Shard(const std::string& file);
    ~Shard();
    void run();

private:
    size_t x, y;
    char mode;
    std::string status;
    std::string section;
    std::string filename;
    std::vector<std::string> lines;
    std::string clipboard;
    int color_pair;
    size_t scroll_offset;
    
    Selection select_coords;
    bool selecting;

    void update();
    void statusline();
    void print();
    void input(int c);

    void up();
    void right();
    void left();
    void down();

    void m_remove(int number);
    std::string m_tabs(std::string& line);
    void m_insert(std::string line, int number);
    void m_append(std::string& line);

    void open();
    void save();
    
    std::string get_selected_text();
    void clear_selection();
    void start_selection();
    void update_selection();
    void delete_selected_text();

    void paste_at_cursor();
    void paste_before_line();
    void paste_after_line();
};

#endif
