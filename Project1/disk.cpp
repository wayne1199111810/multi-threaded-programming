#include <iostream>
#include <fstream>
#include <string>
#include <climits>   // INT_MIN
#include <stdlib.h>  //abs, atoi
#include <vector>
#include "thread.h"

using namespace std;

struct Node
{
	int track;
	int requester;
	Node *next;
};

struct queue
{
	int count = 0;
	int current_track = 0;
	Node *head;
	void enqueue(int track, int requester);
	Node dequeue();
	void print();
};

int max_que;
int alive_thread = 0;
int numberinput;
queue q;
mutex mutex1;
cv service_cv,deque_cv, enque_cv;
vector<bool> deadthread;
vector<bool> can_serve;

void queue::print()
{
	Node *ptr = this->head->next;
	mutex1.lock();
	while(ptr != NULL)
	{
		cout << ptr->requester<< ", " << ptr->track<<"; ";
		ptr = ptr->next;
	}
	cout<<endl;
	mutex1.unlock();
}

void queue::enqueue(int track, int requester)
{
	///////////////////
	while(this->count >= max_que)
	{
		enque_cv.wait(mutex1);
		//wait
	}
	///////////////////
	//add new element to the queue
	if (this->count < max_que)
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
		cout << "requester " << requester << " track " << track << endl;
		this->count++;
		deque_cv.signal();
		enque_cv.signal();
		service_cv.signal();
	}
	//signal dequeue if (queue is full) or (alive request <= max_que)
}

Node queue::dequeue()
{
	//wait until the (queue is full) or (alive request <= max_que)
	while(alive_thread >= max_que && this->count < max_que)
	{
		//wait
		deque_cv.wait(mutex1);
	}
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
	this->count--;
	enque_cv.signal();
	deque_cv.signal();
	service_cv.signal();
	return answer;
}

void request(char *a)
{
	// open file
	string filename = (char *)a;
	ifstream f;
	f.open(filename);
	// read track from input file
	if(f.is_open())
	{
		string line;
		int length = filename.find_last_of(".") - filename.find_first_of(".") - 3;
		int requester = atoi(filename.substr(7, length).c_str());
		
		mutex1.lock();
		alive_thread++;
		while(getline(f, line))
		{
			int track = atoi(line.c_str());
			// adding the current track into queue
			///////////////

			q.enqueue(track, requester);
			can_serve[requester] = false;
			///////////////
		}
		f.close();
		deadthread[requester] = true;
		mutex1.unlock();
	}
}

void service(void **argv)
{

	deadthread = vector<bool>(numberinput-2);
	can_serve = vector<bool>(numberinput-2);
	for(int i=0; i<numberinput-2; i++)
	{
		deadthread[i] = false;
		can_serve[i] = true;
	}
	// initialize queue;
	Node *head = new Node;
	head->next = NULL;
	head->track = INT_MIN;
	q.head = head;
	// handling every request
	for(int i=2; i<numberinput; i++)
	{
		char* filename = (char*) argv[i];
		thread t ((thread_startfunc_t) request, (char *) filename);
	}
	mutex1.lock();
	while(alive_thread > 0)
	{
		// when we can add more Node to the queue
		while(q.count < max_que && alive_thread >= max_que)
		{
			//wait
			service_cv.wait(mutex1);
		}
		if (q.count > 0)
		{
			Node n = q.dequeue();
			cout<<"service requester "<< n.requester <<" track "<< n.track << endl;
			if (deadthread[n.requester])
			{
				alive_thread--;
			}
		}
	}
	mutex1.unlock();
	// Finishing all the requesters
}

int main(int argc, char **argv)
{
	numberinput = argc;

	if (argc >= 3)
	{
		max_que = atoi(argv[1]);
		cpu::boot((thread_startfunc_t) service, (char **) argv, 0);
	}
	else
	{
		cout<<"Please input with file names" <<endl;
	}
}

