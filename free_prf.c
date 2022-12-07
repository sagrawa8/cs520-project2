#include <stdio.h>
# define SIZE 100

void enqueue();
void dequeue();
void show();
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
 
void dequeue()
{
    if (Front == - 1 || Front > Rear)
    {
        printf("Underflow \n");
        return ;
    }
    else
    {
        printf("Element deleted from the Queue: %d\n", prf_free_queue[Front]);
        Front = Front + 1;
    }
} 
 
void show()
{
    
    if (Front == - 1)
        printf("Empty Queue \n");
    else
    {
        printf("Queue: \n");
        for (int i = Front; i <= Rear; i++)
            printf("%d ", prf_free_queue[i]);
        printf("\n");
    }
}