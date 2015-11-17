#include <iostream>
#include "../includes/cbwindow.hpp"
int main(int argc, char *argv[])
{
    CBWindow* win=new CBWindow(10,5);
    tuple_t tupla;
    tupla.ask_price=10;
    tupla.ask_size=1;
    winresult_t res;
    for(int i=0;i<5;i++)
    {
        tupla.id=i;
        tupla.ask_price=10+i;
        tupla.original_timestamp=i;
        win->insert(tupla);
        std::cout<< "is computable? "<<win->isComputable()<<std::endl;
    }
    win->printAll();
    std::cout<< "is computable? "<<win->isComputable()<<std::endl;
    win->compute(res);
    std::cout<< "Open ask "<<res.open_ask <<"Close ask "<<res.close_ask << "High "<<res.high_ask<<std::endl;
    std::cout<< res.p0_ask<<" + "<<res.p1_ask<<"x + "<<res.p2_ask<<"x*x"<<std::endl;



}

