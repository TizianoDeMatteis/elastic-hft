/*
 * ---------------------------------------------------------------------

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
#ifndef WINDOW_H
#define WINDOW_H

/**
    Abstract definition of a generic Window
    @param T type of window's elements
    @param RT type of computation element
*/

template<typename T, typename RT>
class Window{

public:
    /**
     * @brief insert an element into the window
     */
    virtual void insert (const T &) =0;

    /**
     * @brief compute the elements in window
     */
    virtual void compute(RT&)=0;

    /**
     * @brief ~Window default destructor
     */
    virtual ~Window(){}

protected:
    /**
     * Elements in window
     */
    T* elements;
};

#endif // WINDOW_H

