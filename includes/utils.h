/**
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
  Contains the declarations of various utility functions used throughout
  the whole program
 */
#ifndef UTILS_H
#define UTILS_H
#include <map>
#include <vector>
#include <string>
/**
 * @brief readVoltagesMap read the voltages Map from the file
 * @param file_name file that contains the voltages. It must be created through the Mammut library
 * @return a map, with key a pair containing the number of active cores and frequency and value the voltage
 */

std::map<std::pair<int,int>,double>  *loadVoltageTable(std::string fileName);

/**
 * @brief getCoreIDs returns the ids of cores that can be used for
 * @return a vector containing the ids of the core that can be used for running the program

*/
std::vector<int>* getCoreIDs();

/**
 * @brief getMaximumFrequency returns the maximum nominal frequency of the machine
 * @return the frequency in MHz
 */
float getMaximumFrequency();

/**
 * @brief getMinimumFrequency
 * @return the frequency in MHz
 */
float getMinimumFrequency();

std::string intToString(int x);

#endif // UTILS_H

