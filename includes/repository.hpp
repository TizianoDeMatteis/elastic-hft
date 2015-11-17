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

   Contains the definition of the repository (aka Backing Store) that will be used
   by the Worker for exchanging windows during the reconfiguration phase and to notify
   to the controller that this phase has finished.

   For the moment being it is specifically tailored for shared memory architectures (we will
   exchange pointers) and count based window.
   As future works, more efficient implementation can be found and the repository can be
   implemented as a templated class in order to be a little bit more generic. Moreover
   we can allow a dynamic number of classes to be moved (for example my using a map instead
   of a vector of references)

   PLEASE NOTE: the correctness of interactions between the various entity is assured by
   the reconfiguration protocol. The repository is not thread safe itself or accessible in
   mutual exclussion.

*/
#ifndef REPOSITORY_HPP
#define REPOSITORY_HPP
#include "cbwindow.hpp"
class Repository{
public:
    /**
     * @brief Repository constructor. It will be iniatilized to synchronize and move classes
     * between max_entities at most.
     * @param max_entities maximum number of workers
     * @param num_classes maximum number of classes (key) that may be moved
     */
    Repository(int max_entities, int num_classes)
    {
        max_workers=max_entities;
        nc=num_classes;
        moving_windows=new CBWindow*[num_classes]();
        reconfiguration_finished=new bool[max_entities]();
        has_to_move_out=new bool[max_entities]();
        //set all window entries to nullptr
        for(int i=0;i<num_classes;i++)
            moving_windows[i]=nullptr;
        //set to true all workers flag
        for(int i=0;i<max_workers;i++)
        {
            reconfiguration_finished[i]=true;
            has_to_move_out[i]=false;
        }
    }

    ~Repository()
    {
        delete moving_windows; //do not delete windows if present
        delete []reconfiguration_finished;
    }


    /**
     * @brief isWindowPresent returns true if the desidered window is present into the repository
     * @param class_id class id
     * @return true if the window is present, false otherwise
     */
    bool isWindowPresent(int class_id)
    {
        return moving_windows[class_id]!=nullptr;
    }

    /**
     * @brief getWindow returns a class with given key if present into the repository.
     * If the operation succed, the entry into the repository is set to nullptr
     * @param class_id id of the class that we are looking for
     * @return the Window if present into the repository, nullptr otherwise
     */
    CBWindow* getAndRemoveWindow(int class_id)
    {
        if(moving_windows[class_id])
        {
            CBWindow * ret=moving_windows[class_id];
            moving_windows[class_id]=nullptr;
            return ret;
        }
        else
            return nullptr;
    }

    /**
     * @brief setWindow save the reference of the Window relative to a given class into the repository
     * @param class_id class type of the window that we want to move through the repository
     * @param window reference to the window that we want to move
     */
    void setWindow(int class_id, CBWindow *window)
    {
        moving_windows[class_id]=window;
    }


    /**
     * @brief setWorkerFinished set a boolean value stating if a particular worker has finished its reconfiguration or not
     * @param id, id of the Worker (we assume that they are in the range [0,max_workers-1]
     * @param finished boolean value
     */
    void setWorkerFinished(int id, bool finished)
    {
        reconfiguration_finished[id]=finished;
    }

    /**
     * @brief setHasToMoveOut specifies if a certaing worker has some class to be moved out. Used mainly for testing
     * various protocol of stop migration
     * @param id of the worker
     * @param flag true if the Worker has to migrate some state
     */
    void setHasToMoveOut(int id,bool flag)
    {
        has_to_move_out[id]=flag;
    }

    /**
     * @brief waitUntilMovedOut actively wait until all the workers have moved out
     * their state during a reconfiguration that involve state migration
     * Used mainly for testing various protocol of stop migration
     */
    void waitUntilMovedOut()
    {
        for(int i=0;i<max_workers;i++)
        {
            while(has_to_move_out[i])
            {
                asm volatile("PAUSE" ::: "memory");
            }
        }
    }


    /**
     * @brief hasWorkerFinished returns a boolean value stating if a particular Worker has finished
     * its reconfiguration phase or not
     * @param id of the Worker
     * @return true if the Worker has finished the reconfiguration phase, false otherwise
     */
    bool hasWorkerFinished(int id)
    {
        return reconfiguration_finished[id];
    }

    /**
     * @brief waitReconfFinished waits until all the involved workers have finished reconfiguration
     */
    void waitReconfFinished()
    {
        for(int i=0;i<max_workers;i++)
        {
            while(!reconfiguration_finished[i])
            {
                asm volatile("PAUSE" ::: "memory");
            }
        }
    }


private:
    int max_workers;
    int nc;
    CBWindow **moving_windows;
    //ticks moving_time[3000]; //just for testing
//    unordered_map<int,CBWindow*> *moving_windows; //map that contains the various windows that have to be moved
    bool *reconfiguration_finished; //prima era un atomic int....non ricordo perche' (era anche allineato)
    bool *has_to_move_out;

};

#endif // REPOSITORY_HPP

