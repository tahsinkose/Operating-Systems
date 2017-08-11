#include "aux_funcs.h"
struct node
{
    int data;
    struct node *next;
}*head;
 
 
 
void append(int num)
{
    struct node *temp,*right;
    temp= (struct node *)malloc(sizeof(struct node));
    temp->data=num;
    right=(struct node *)head;
    while(right->next != NULL)
    right=right->next;
    right->next =temp;
    right=temp;
    right->next=NULL;
}
 
 
 
void add( int num )
{
    struct node *temp;
    temp=(struct node *)malloc(sizeof(struct node));
    temp->data=num;
    if (head== NULL)
    {
    head=temp;
    head->next=NULL;
    }
    else
    {
    temp->next=head;
    head=temp;
    }
}
void addafter(int num, int loc)
{
    int i;
    struct node *temp,*left,*right;
    right=head;
    for(i=1;i<loc;i++)
    {
    left=right;
    right=right->next;
    }
    temp=(struct node *)malloc(sizeof(struct node));
    temp->data=num;
    left->next=temp;
    left=temp;
    left->next=right;
    return;
}
 
int size()
{
    struct node *n;
    int c=0;
    n=head;
    while(n!=NULL)
    {
    n=n->next;
    c++;
    }
    return c;
} 
 
void insert_process(int num)
{
    int c=0;
    struct node *hold;
    hold=head;
    if(hold==NULL)
    {
    	add(num);
    }
    else
    {
    	while(hold!=NULL)
    	{
    	    if(hold->data < num)
        	c++;
       	 	hold=hold->next;
    	}
    	if(c==0)
        	add(num);
    	else if(c < size())
        	addafter(num,++c);
    	else
        	append(num);
   }
}
 
 
 
int remove_process(int num)
{
    struct node *hold, *prev;
    hold=head;
    while(hold!=NULL)
    {
    	if(hold->data==num)
    	{
        	if(hold==head)
        	{
        		head=hold->next;
        		free(hold);
        		return 1;
        	}
        	else
        	{
        		prev->next=hold->next;
        		free(hold);
        		return 1;
        	}
    	}
   	else
    	{
        	prev=hold;
        	hold= hold->next;
    	}
    }
    return 0;
}
 
 
void  list_bg_processes(struct node *r)
{
    r=head;
    if(r==NULL)
    {
    	return;
    }
    while(r!=NULL)
    {
    	printf("[%d]\n",r->data);
    	r=r->next;
    }
}
 
