#include "Board.hpp"
#include <climits>
#include <cstdio>

void printbits(uint64_t n){
    uint64_t i; 
    i = (uint64_t)1 << (sizeof(n)*CHAR_BIT-1);
    while (i > 0){
        if(n&i)
            printf("1"); 
        else 
            printf("0"); 
        i >>= 1;
    }
}


int main(){

    Board board = Board();
    board.InitializeBoard();
    printbits(board.GetColorOccupation(1));


    return 0;
}