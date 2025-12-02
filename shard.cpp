#include "shard.hpp"
#include "clocale"
#include <fstream>
#include <sys/stat.h>
#include <ncurses.h>
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <csignal>
#include <termios.h>
#include <unistd.h>

void Shard::paste_at_cursor() {
    if (clipboard.empty() || y >= lines.size()) {
        status = " CLIPBOARD EMPTY ";
        color_pair = 5;
        return;
    }

    if (select_coords.start.y != -1) {
        delete_selected_text();
        clear_selection();
        selecting = false;
    }

    size_t prev_x = x;
    std::string current_line = clipboard;
    std::string next_line_part = lines[y].substr(x);
    size_t pos = 0;
    
    while ((pos = current_line.find('\n')) != std::string::npos) {
        std::string part = current_line.substr(0, pos);
        lines[y].insert(x, part);
        x += part.length();

        m_insert(lines[y].substr(x) + next_line_part, static_cast<int>(y + 1));
        lines[y].erase(x);
        
        y++;
        x = 0;
        next_line_part = ""; 
        current_line.erase(0, pos + 1);
    }
    
    lines[y].insert(x, current_line);
    x += current_line.length();

    status = " PASTED: " + std::to_string(clipboard.length()) + " chars ";
    color_pair = 4;
}

void Shard::paste_before_line() {
    if (clipboard.empty() || lines.empty()) {
        status = " CLIPBOARD EMPTY ";
        color_pair = 5;
        return;
    }

    clear_selection();
    selecting = false;
    
    size_t paste_row = y;
    
    if (paste_row == 0 && lines[0].empty()) {
        paste_at_cursor();
        return;
    }

    x = 0;
    
    std::string buffer = clipboard;
    std::string line;
    size_t pos = 0;
    
    while ((pos = buffer.find('\n')) != std::string::npos) {
        line = buffer.substr(0, pos);
        m_insert(line, static_cast<int>(paste_row));
        paste_row++;
        buffer.erase(0, pos + 1);
    }
    
    if (!buffer.empty()) {
        m_insert(buffer, static_cast<int>(paste_row));
        paste_row++;
    }

    y = paste_row - (clipboard.find('\n') == std::string::npos ? 1 : 0);
    x = 0; 

    status = " PASTED (BEFORE): " + std::to_string(clipboard.length()) + " chars ";
    color_pair = 4;
}

void Shard::paste_after_line() {
    if (clipboard.empty() || lines.empty()) {
        status = " CLIPBOARD EMPTY ";
        color_pair = 5;
        return;
    }

    clear_selection();
    selecting = false;
    
    size_t paste_row = y + 1;
    
    x = 0;
    
    std::string buffer = clipboard;
    std::string line;
    size_t pos = 0;
    
    while ((pos = buffer.find('\n')) != std::string::npos) {
        line = buffer.substr(0, pos);
        m_insert(line, static_cast<int>(paste_row));
        paste_row++;
        buffer.erase(0, pos + 1);
    }
    
    if (!buffer.empty()) {
        m_insert(buffer, static_cast<int>(paste_row));
        paste_row++;
    }

    y = y + 1;	
    x = 0; 

    status = " PASTED (AFTER): " + std::to_string(clipboard.length()) + " chars ";
    color_pair = 4;
}

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
    selecting = false;

    if (file.empty()){
        filename = "Untitled";
    }else{
        filename = file;
    }

    setlocale(LC_ALL, "");
    initscr();
    noecho();

    struct termios old_t, new_t;
    tcgetattr(STDIN_FILENO, &old_t);
    new_t = old_t;
    new_t.c_iflag &= ~(IXON | IXOFF);
    new_t.c_lflag &= ~(ICANON | ECHO);
    new_t.c_cc[VMIN] = 1;
    new_t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_t);

    cbreak();
    keypad(stdscr, true);
    intrflush(stdscr, FALSE);
    signal(SIGINT, SIG_IGN);

    if (has_colors()){
        start_color();
        init_pair(1, COLOR_BLACK, COLOR_GREEN);
        init_pair(2, COLOR_BLACK, COLOR_MAGENTA);
        init_pair(3, COLOR_WHITE, COLOR_BLACK);
        init_pair(4, COLOR_BLACK, COLOR_WHITE);
        init_pair(5, COLOR_BLACK, COLOR_MAGENTA);
        init_pair(6, COLOR_BLACK, COLOR_YELLOW);
    }
    			
    select_coords.start.y = -1;
    select_coords.start.x = -1;
    select_coords.end.y = -1;
    select_coords.end.x = -1;

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
    if (status.find("ERROR") != std::string::npos || status.find("SAVED") != std::string::npos || 
        status.find("COPIED") != std::string::npos || status.find("PASTED") != std::string::npos ||
        status.find("CLIPBOARD") != std::string::npos || status.find("SELECTION") != std::string::npos ||
        status.find("CUT") != std::string::npos) {
        if (mode == 'n' && (status.find("SAVED") != std::string::npos || 
            status.find("COPIED") != std::string::npos || status.find("PASTED") != std::string::npos || status.find("CUT") != std::string::npos)) {	
             status = " NORMAL ";	
             color_pair = 2;
        } else if (mode == 'i' && (status.find("SAVED") != std::string::npos || 
            status.find("COPIED") != std::string::npos || status.find("PASTED") != std::string::npos || status.find("CUT") != std::string::npos)) {	
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

void Shard::start_selection() {
    if (select_coords.start.y == -1) {
        select_coords.start.x = static_cast<int>(x);
        select_coords.start.y = static_cast<int>(y);
        select_coords.end.x = static_cast<int>(x);
        select_coords.end.y = static_cast<int>(y);
        selecting = true;
    }
}

void Shard::update_selection() {
    if (select_coords.start.y != -1) {
        select_coords.end.x = static_cast<int>(x);
        select_coords.end.y = static_cast<int>(y);
        selecting = true;
    }
}

void Shard::delete_selected_text() {
    if (select_coords.start.y == -1) return;
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
        return;
    }
    
    if (start_y == end_y) {
        if (end_x > start_x) {
            lines[start_y].erase(start_x, end_x - start_x);
        }
        x = start_x;
        y = start_y;
    } else {
        std::string remaining = lines[start_y].substr(0, start_x) +
                                lines[end_y].substr(end_x);
        		
        for (size_t i = end_y; i > start_y; --i) {
            m_remove(static_cast<int>(i));
        }
        		
        lines[start_y] = remaining;
        x = start_x;
        y = start_y;
    }
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
         	clear_selection();
         	selecting = false;
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
                    if (!selecting) clear_selection();
                    break;
                case KEY_DOWN:
                    down();
                    if (!selecting) clear_selection();
                    break;
                case KEY_LEFT:
                    left();
                    if (!selecting) clear_selection();
                    break;
                case KEY_RIGHT:
                    right();
                    if (!selecting) clear_selection();
                    break;
                
                case 23: 
                    if (y > 0) {
                        if (!selecting) {
                            start_selection();
                            select_coords.start.x = 0;
                            select_coords.end.x = lines[y].length();
                        }
                        
                        up(); 
                        
                        if (y < static_cast<size_t>(select_coords.start.y)) {
                            select_coords.start.y = static_cast<int>(y);
                            select_coords.start.x = 0;	
                        } else {
                            select_coords.end.y = static_cast<int>(y);
                            select_coords.end.x = lines[y].length();
                        }

                        if (y < scroll_offset) {
                            --scroll_offset;
                        }
                    }
                    break;

                case 19: 
                    if (y < lines.size() - 1) {
                        if (!selecting) {
                            start_selection();
                            select_coords.start.x = 0;
                            select_coords.end.x = lines[y].length();
                        }

                        down(); 
                        
                        if (y > static_cast<size_t>(select_coords.start.y)){
                            select_coords.end.y = static_cast<int>(y);
                            select_coords.end.x = lines[y].length();
                        } else {
                            select_coords.start.y = static_cast<int>(y);
                            select_coords.start.x = 0;
                        }

                        size_t screen_height = LINES - 1;
                        if (y >= scroll_offset + screen_height) {
                            ++scroll_offset;
                        }
                    }
                    break;
                
                case 1: 
                    if (!selecting) {
                        start_selection();
                    }
                    left();
                    update_selection();
                    break;
                case 4: 
                    if (!selecting) {
                        start_selection();
                    }
                    right();
                    update_selection();
                    break;
                
                case 17: 
                    if (select_coords.start.y != -1) {
                        clipboard = get_selected_text();
                        delete_selected_text();
                        clear_selection();
                        selecting = false;
                        status = " CUT: " + std::to_string(clipboard.length()) + " chars ";
                        color_pair = 5;	
                    } else {
                        status = " NO SELECTION TO CUT ";
                        color_pair = 5;
                    }
                    break;

                case 11: 
                    if (select_coords.start.y != -1) {
                        clipboard = get_selected_text();
                        if (!clipboard.empty()) {
                            status = " COPIED: " + std::to_string(clipboard.length()) + " chars ";
                            color_pair = 4;
                        } else {
                            status = " COPY FAILED ";
                            color_pair = 5;
                        }
                        clear_selection();
                        selecting = false;
                    } else if (y < lines.size()) {
                        clipboard = lines[y]; 
                        status = " COPIED LINE: " + std::to_string(clipboard.length()) + " chars ";
                        color_pair = 4;
                    }
                    break;
                    
                case 22: 
                    paste_at_cursor();
                    break;
                
                case 25: 
                    paste_before_line();
                    break;
                
                case 16: 
                    paste_after_line();
                    break;
                    
                case 127:
                case KEY_BACKSPACE:
                    if (select_coords.start.y != -1) {
                        delete_selected_text();
                        clear_selection();
                        selecting = false;
                    } else {
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
                    }
                    break;

                case KEY_ENTER:
                case 10:
                    if (select_coords.start.y != -1) {
                        delete_selected_text();
                        clear_selection();
                        selecting = false;
                    }
                                             
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
                        if (select_coords.start.y != -1) {
                            delete_selected_text();
                            clear_selection();
                            selecting = false;
                        }
                                                
                        if (y < lines.size()) {
                            lines[y].insert(x, 1, static_cast<char>(c));
                            ++x;
                        }
                    } else {
                        if (selecting) {
                            selecting = false;
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
            if (select_coords.start.y != -1 && selecting) {
                Coords start = select_coords.start;
                Coords end = select_coords.end;
                			
                if (start.y > end.y || (start.y == end.y && start.x > end.x)) {
                    std::swap(start, end);
                }
                			
                size_t line_y = buffer_index;
                const std::string& current_line = lines[buffer_index];
                			
                if (line_y >= static_cast<size_t>(start.y) && 
                    line_y <= static_cast<size_t>(end.y)) {

                    size_t sel_start = (line_y == static_cast<size_t>(start.y)) ? start.x : 0;
                    size_t sel_end = (line_y == static_cast<size_t>(end.y)) ? end.x : current_line.length();

                    sel_end = std::min(sel_end, current_line.length());
                    sel_start = std::min(sel_start, current_line.length());
                    					
                    if (sel_start > 0) {
                        mvprintw(static_cast<int>(i), 0, "%.*s", 
                                 static_cast<int>(sel_start), 
                                 current_line.c_str());
                    }
                    					
                    if (sel_end > sel_start) {
                        attron(A_REVERSE);
                        size_t len = sel_end - sel_start;
                        mvprintw(static_cast<int>(i), static_cast<int>(sel_start), "%.*s", 
                                 static_cast<int>(len), 
                                 current_line.c_str() + sel_start);
                        attroff(A_REVERSE);
                    }
                    					
                    if (sel_end < current_line.length()) {
                        mvprintw(static_cast<int>(i), static_cast<int>(sel_end), "%s", 
                                 current_line.c_str() + sel_end);
                    }
                } else {
                    mvprintw(static_cast<int>(i), 0, current_line.c_str());
                }
            } else {
                mvprintw(static_cast<int>(i), 0, lines[buffer_index].c_str());
            }
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
        return "";
    }

    if (start_y == end_y) {
        if (start_x < lines[start_y].length()) {
            size_t len = end_x > start_x ? end_x - start_x : 0;
            if (start_x + len > lines[start_y].length()) {
                len = lines[start_y].length() - start_x;
            }
            selected_text = lines[start_y].substr(start_x, len);
        }
    } else {
        if (start_x < lines[start_y].length()) {
            selected_text += lines[start_y].substr(start_x);
        }
        selected_text += '\n';

        for (size_t i = start_y + 1; i < end_y && i < lines.size(); ++i) {
             selected_text += lines[i] + '\n';
        }
        		
        if (end_y < lines.size() && end_x > 0) {
            size_t len = end_x;
            if (len > lines[end_y].length()) {
                len = lines[end_y].length();
            }
            selected_text += lines[end_y].substr(0, len);
        }
    }
    return selected_text;
}

void Shard::clear_selection(){
    select_coords.start.y = -1;
    select_coords.start.x = -1;
    select_coords.end.y = -1;
    select_coords.end.x = -1;
}
