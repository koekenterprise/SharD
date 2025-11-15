#include "shard.hpp"
#include "clocale"
#include <fstream>
#include <sys/stat.h>
#include <ncurses.h>
#include <algorithm>
#include <stdexcept>
#include <iostream>

void Shard::open() {
    struct stat buffer;

    if (stat(filename.c_str(), &buffer) == 0){

        std::ifstream ifile(filename);
        if (ifile.is_open()){
            std::string buffer_line;

            bool empty_file = true;
            while(std::getline(ifile, buffer_line)){
                m_append(buffer_line);
                empty_file = false;
            }

            if (empty_file){
                 lines.push_back("");
            }
            ifile.close();

        } else {
            throw std::runtime_error("Could not open file. Permission denied! File: " + filename);
        }
    } else {
        std::string str {};
        m_append(str);
    }

    if (lines.empty()) {
        lines.push_back("");
    }
}

void Shard::save(){
    std::ofstream ofile(filename);
    if(ofile.is_open()){

        for (size_t i {}; i < lines.size(); ++i){
            ofile << lines[i];

            if (i < lines.size() - 1) {
                ofile << '\n';
            }
        }

        ofile.close();

        status = " SAVED ";
        color_pair = 4;

    } else {

        status = " ERROR: Permission denied! File: " + filename;
        color_pair = 5;
    }
}

Shard::Shard(const std::string& file){

    x = y = 0;
    mode = 'n';
    status = "NORMAL";
    section = {};
   
    scroll_offset = 0; 

    if (file.empty()){
        filename = "Untitled";
    }else{
        filename = file;
    }

    setlocale(LC_ALL, "");
    initscr();
    noecho();
    cbreak();
    keypad(stdscr, true);

    if (has_colors()){
        start_color();
        init_pair(1, COLOR_BLACK, COLOR_GREEN);
        init_pair(2, COLOR_BLACK, COLOR_YELLOW);
        init_pair(3, COLOR_WHITE, COLOR_BLACK);
        init_pair(4, COLOR_BLACK, COLOR_WHITE);
        init_pair(5, COLOR_BLACK, COLOR_MAGENTA);
        init_pair(6, COLOR_BLACK, COLOR_YELLOW);
    }
    
    
    select_coords.start.y = -1;

    try {
        open();
    } catch (const std::runtime_error& e) {
        endwin();
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        throw;
    }

    clipboard = "";
}

Shard::~Shard(){
    endwin();
}

void Shard::run(){
    refresh();
    while(mode != 'q'){
        update();
        statusline();
        print();
        int c = getch();
        input(c);
    }
}

void Shard::update(){

    if (status.find("ERROR") != std::string::npos || status.find("SAVED") != std::string::npos) {

        if (mode == 'n' && status.find("SAVED") != std::string::npos) {
             status = " NORMAL ";
             color_pair = 2;
        } else if (mode == 'i' && status.find("SAVED") != std::string::npos) {
             status = " INSERT ";
             color_pair = 1;
        }

    } else {
        switch (mode){
            case 27:
            case 'n':
                status = " NORMAL ";
                color_pair = 2;
                break;
            case 'i':
                status = " INSERT ";
                color_pair = 1;
                break;
            case 'q':
                break;
        }
    }

    section = " | COLS: " + std::to_string(x) + " | ROWS: " + std::to_string(y) + " | FILE: " + filename + " | SharD ";
}

void Shard::statusline(){
    attron(COLOR_PAIR(color_pair));
    attron(A_BOLD);

    mvprintw(LINES - 1, 0, "%*s", COLS, "");

    mvprintw(LINES - 1, 0, status.c_str());
    mvprintw(LINES - 1, static_cast<int>(status.length()), " ⵙ)");
    mvprintw(LINES - 1, COLS - static_cast<int>(section.length()), section.c_str());

    attroff(A_BOLD);
    attroff(COLOR_PAIR(color_pair));

    
    move(static_cast<int>(y - scroll_offset), static_cast<int>(x)); 
    refresh();
}


void Shard::input(int c){


    switch (c) {
        case 'q':
            if (mode == 'n') {
                mode = 'q';
                return;
            }
            
            if (mode == 'i') { 
                break;
            }
            return; 

        case 'i':
            if (mode != 'i') { 
                mode = 'i';
                return; 
            }
            break; 

        case 27: 
            mode = 'n';
            return; 

        case 15: 
            if (mode == 'n'){
                save();
            }
            return; 
            
        default:
            break; 
    }

    switch (mode){
        case 'n':
            switch (c) {
                case 'w':
                case 'W':
                case 'k':
                    up();
                    break;
                case 's':
                case 'S':
                case 'j':
                    down();
                    break;
                case 'a':
                case 'A':
                case 'h':
                    left();
                    break;
                case 'd':
                case 'D':
                case 'l':
                    right();
                    break;
            }
            break;

        case 'i':
            switch(c){
                
                case KEY_UP:
                    up();
                    break;
                case KEY_DOWN:
                    down();
                    break;
                case KEY_LEFT:
                    left();
                    break;
                case KEY_RIGHT:
                    right();
                    break;

                case 11:
                    if (y < lines.size()){
                        clipboard = lines[y].substr(x);
                        status = " COPIED: " + std::to_string(clipboard.length()) + " chars ";
                        color_pair = 4;
                    }
                    break;

                case 25: 
                    if (!clipboard.empty() && y < lines.size()) {
                        lines[y].insert(x, clipboard);
                        x += clipboard.length();
                        status = " PASTED ";
                        color_pair = 4;
                    }
                    break;

                case 127:
                case KEY_BACKSPACE:
                    if (x == 0 && y > 0){
                        if (y-1 < lines.size()) {
                            x = lines[y-1].length();
                            lines[y-1] += lines[y];
                            m_remove(static_cast<int>(y));
                            --y;
                        }
                    }
                    else if (x > 0 && y < lines.size()){
                        lines[y].erase(--x, 1);
                    }
                    break;

                case KEY_ENTER:
                    case 10:
                        if (y < lines.size()) {
                            if (x < lines[y].length()){
                                size_t chars_to_move = lines[y].length() - x;
                                m_insert(lines[y].substr(x, chars_to_move), static_cast<int>(y + 1));
                                lines[y].erase(x, chars_to_move);
                            } else {
                                m_insert("", static_cast<int>(y + 1));
                            }
                    
                            x = 0;
                           
                            down(); 
                        }
                        break;

                case KEY_BTAB:
                case KEY_CTAB:
                case KEY_STAB:
                case KEY_CATAB:
                case 9:
                    if (y < lines.size()) {
                        lines[y].insert(x, 2, ' ');
                        x += 2;
                    }
                    break;

                case '(':
                    if (y < lines.size()) {
                        lines[y].insert(x, 2, ')');
                        lines[y].replace(x, 1, "(");
                        ++x;
                    }
                    break;

                case '[':
                    if (y < lines.size()) {
                        lines[y].insert(x, 2, ']');
                        lines[y].replace(x, 1, "[");
                        ++x;
                    }
                    break;

                case '{':
                    if (y < lines.size()) {
                        lines[y].insert(x, 2, '}');
                        lines[y].replace(x, 1, "{");
                        ++x;
                    }
                    break;
                
                
                default:
                    if ((c >= 32 && c <= 126) || c > 255) {
                        if (y < lines.size()) {
                            lines[y].insert(x, 1, static_cast<char>(c));
                            ++x;
                        }
                    }
            }
            break;
    }
}

void Shard::print(){
   
    for (size_t i {}; i < (size_t)LINES-1; ++i){
        size_t buffer_index = i + scroll_offset;
        
        if (buffer_index >= lines.size()){
           
            move(static_cast<int>(i), 0);
            clrtoeol();
        } else {
          
            mvprintw(static_cast<int>(i), 0, lines[buffer_index].c_str());
        }
        clrtoeol();
    }

    
    move(static_cast<int>(y - scroll_offset), static_cast<int>(x)); 
}

void Shard::m_remove(int number){
    if (number >= 0 && static_cast<size_t>(number) < lines.size()) {
        lines.erase(lines.begin() + number);
    }
}

std::string Shard::m_tabs(std::string& line){
    std::size_t pos = 0;
    while ((pos = line.find('\t')) != std::string::npos) {
        line.replace(pos, 1, " ");
    }
    return line;
}

void Shard::m_insert(std::string line, int number){
    line = m_tabs(line);
    size_t insert_pos = (number >= 0 && static_cast<size_t>(number) <= lines.size()) ? static_cast<size_t>(number) : lines.size();
    lines.insert(lines.begin() + insert_pos, line);
}

void Shard::m_append(std::string& line){
    line = m_tabs(line);
    lines.push_back(line);
}

void Shard::up(){
    if(y > 0){
        --y;
    }

   
    if (y < scroll_offset) {
      
        --scroll_offset;
    }

    if (y < lines.size() && x > lines[y].length()){
        x = lines[y].length();
    }

    
}

void Shard::right(){

    if (y < lines.size() && x < lines[y].length()){
        ++x;
      
    }
}

void Shard::left(){
    if(x > 0){
        --x;
   
    }
}

void Shard::down(){
    size_t screen_height = LINES - 1;

    if(y < lines.size() - 1){
        ++y;
    }

    if (y >= scroll_offset + screen_height) {
       
        ++scroll_offset;
    }

    if(y < lines.size() && x > lines[y].length()){
        x = lines[y].length();
    }
   
}

std::string Shard::get_selected_text(){
    if (select_coords.start.y == -1) return "";

    std::string selected_text = "";
    Coords start = select_coords.start;
    Coords end = select_coords.end;

    if (start.y > end.y || (start.y == end.y && start.x > end.x)) {
        std::swap(start, end);
    }

    size_t start_y = static_cast<size_t>(start.y);
    size_t start_x = static_cast<size_t>(start.x);
    size_t end_y = static_cast<size_t>(end.y);
    size_t end_x = static_cast<size_t>(end.x);
    
    if (start_y >= lines.size() || end_y >= lines.size()) {
        clear_selection();
        return "";
    }

    if (start_y == end_y) {
        size_t len = end_x > start_x ? end_x - start_x : 0;
        selected_text = lines[start_y].substr(start_x, len);
    } else {
        selected_text += lines[start_y].substr(start_x) + '\n';
        
        for (size_t i = start_y + 1; i < end_y; ++i) {
             selected_text += lines[i] + '\n';
        }
        
        selected_text += lines[end.y].substr(0, end_x);
    }
    return selected_text;
}

void Shard::clear_selection(){
    select_coords.start.y = -1;
    select_coords.start.x = -1;
    select_coords.end.y = -1;
    select_coords.end.x = -1;
}
