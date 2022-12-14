#include <stdio.h>
# define SIZE 100

void enqueue(int insert_item);
int dequeue();
void show();
int isFreeListEmpty();
int prf_free_queue[SIZE];
int Rear = - 1;
int Front = - 1;


void enqueue(int insert_item)
{
    if (Rear == SIZE - 1)
       printf("Overflow \n");
    else
    {
        if (Front == - 1)
        Front = 0;
        Rear = Rear + 1;
        prf_free_queue[Rear] = insert_item;
    }
} 
 
int dequeue()
{
    int last_val = prf_free_queue[Front];
    printf("Element deleted from the Queue: %d\n", prf_free_queue[Front]);
    Front = Front + 1;
    return last_val;

} 
 
void show()
{
    
    if (Front == - 1){
         //printf("Empty Queue \n");
    }  
    else
    {
        printf("Free PRF Queue: \n");
        for (int i = Front; i <= Rear; i++)
            printf("%d ", prf_free_queue[i]);
        printf("\n");
    }

}

int isFreeListEmpty(){
if(Front == -1 || Front>Rear){
    return 1;
}
else{
    return 0;
}
}

