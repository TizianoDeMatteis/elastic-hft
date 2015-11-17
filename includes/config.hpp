/*
    ---------------------------------------------------------------------

    Copyright (C) 2015- by Tiziano De Matteis (dematteis <at> di.unipi.it)

    This file is part of elastic-hft.

    elastic-hft is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    ---------------------------------------------------------------------
*/
#ifndef CONFIG_HPP
#define CONFIG_HPP
#include <fstream>
#include <map>
/**
 * @brief The Configuration class read a config file
 * with the following syntax:
 * - # comment line
 * - <name> = <value>
 * If the passed file does not respect the syntax an exception is thrown
 */
class Configuration{
public:
    Configuration(std::string const& configFile)
    {
        std::ifstream file(configFile.c_str());
        std::string line;
        std::string name;
        std::string value;
        uint posEqual;
        while (std::getline(file,line))
        {

            if (! line.length()) continue; //empty line
            if (line[0] == '#') continue; //comment
            posEqual=line.find('=');
            if(posEqual==std::string::npos)
                throw std::runtime_error("Bad configuration file");
            name  = trim(line.substr(0,posEqual));

            value = trim(line.substr(posEqual+1));
            //std::cout<< "Name: "<<name << " Value: "<<value<<std::endl;
            _content[name]=value;
        }
    }

    std::string getValue(const std::string &str){
        return _content[str];
    }

private:
    std::string trim(const std::string& str)
    {
        size_t first = str.find_first_not_of(' ');
        size_t last = str.find_last_not_of(' ');
        return str.substr(first, (last-first+1));
    }

    std::map<std::string,std::string> _content;

};

#endif // CONFIG_HPP

