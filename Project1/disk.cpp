#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <climits>   // INT_MIN
#include <stdlib.h>  //abs, atoi
#include "thread.h"

using namespace std;

struct Node
{
	int track;
	string requester;
	Node *next;
};

struct queue
{
	int count = 0;
	int current_track = 0;
	Node *head;
	void enqueue(int track, string requester);
	Node dequeue();
};

int max_disk_queue;
int alive_thread = 0;
int numberfile;
queue q;
mutex mutex1;
cv cv1;

void queue::enqueue(int track, string requester)
{
	///////////////////
	while(this->count >= max_disk_queue)
	{
		//wait
	}
	///////////////////
	//add new element to the queue
	if (this->count < max_disk_queue)
	{
		Node* newelement = new Node;
		newelement->track = track;
		newelement->requester = requester;
		if(this->head->next == NULL)
		{
			newelement->next = NULL;
			head->next = newelement;
		}
		else
		{
			Node *small = this->head;
			Node *large = this->head->next;
			while(large != NULL && track > large->track)
			{
				large = large->next;
				small = small->next;
			}
			if(large == NULL)
			{
				newelement->next = NULL;
				small->next = newelement;			
			}
			else //(track < large->track)
			{
				small->next = newelement;
				newelement->next = large;
			}
		}
		this->count++;
	}
	//signal dequeue if (queue is full) or (alive request <= max_disk_queue)
}

Node queue::dequeue()
{
	//wait until the (queue is full) or (alive request <= max_disk_queue)


	//remove item from queue
	Node *large = this->head->next;
	Node *small = this->head;
	Node answer;
	while(large->next != NULL && this->current_track > large->next->track)
	{
		large = large->next;
		small = small->next;
	}
	if(large->next == NULL)
	{
		answer = (*large);
		small->next = NULL;
		delete large;
	}
	else
	{
		int a = abs(large->next->track - this->current_track);
		int b = abs(large->track - this->current_track);
		if(a < b) // close to large->next
		{
			Node *tmptr = large->next;
			answer = *(tmptr);
			large->next = large->next->next;
			delete tmptr;
		}
		else
		{
			answer = *(large);
			small->next = large->next;
			delete large;
		}
	}
	this->current_track = answer.track;
	//return removed item
	cout << "requester " << answer.requester << " track " << answer.track << endl;
	this->count--;
	return answer;
}

void request(void *a)
{
	// open file
	char *filename = (char *)a;
	stringstream ss;
	ifstream f;
	f.open(filename);
	// read track from input file
	if(f.is_open())
	{
		ss << f.rdbuf();
		////////
		alive_thread++;
		////////
		while(!ss.eof())
		{
			int track;
			ss >> track;
			q.enqueue(track, a);
			// adding the current track into queue
		}
		f.close();
		////////
		alive_thread--;
		////////
	}
}

void service(void **filename)
{
	// initialize queue;
	Node *head = new Node;
	head->next = NULL;
	head->track = INT_MIN;
	q.head = head;
	// handling every request
	for(int i=2; i<numberfile; i++)
	{
		thread t ((thread_startfunc_t) request, (void *) filename[i]);
	}

	while(alive_thread > 0)
	{
		////////////////////////
		// when we can add more Node to the queue
		while(q.count < max_disk_queue && alive_thread > max_disk_queue)
		{
			// wait
		}
		if (q.count > 0)
		{
			Node n = q.dequeue();
			cout << "service requester " << n.requester << " track " << n.track << endl;
		}
		////////////////////////
	}
	// Finishing all the requesters
	cout << "No runnable threads.  Exiting." << endl;
}


void printname(void *a)
{
	cout<<a<<endl;
}

void testhread(void **filename)
{
	for(int i=2; i<numberfile; i++)
	{
		thread t ((thread_startfunc_t) printname, (void *) filename[i]);
	}
}

int main(int argc, char **argv)
{
	max_disk_queue = atoi(argv[1]);
	numberfile = argc;
	cpu::boot((thread_startfunc_t) testhread, (void **) argv, 0);
}