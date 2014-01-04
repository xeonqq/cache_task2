/*
// File: task_1.cpp
//              
// Framework to implement Task 1 of the Advances in Computer Architecture lab 
// session. This uses the ACA 2009 library to interface with tracefiles which
// will drive the read/write requests
//
// Author(s): Michiel W. van Tol, Mike Lankamp, Jony Zhang, 
//            Konstantinos Bousias
// Copyright (C) 2005-2009 by Computer Systems Architecture group, 
//                            University of Amsterdam
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
 */

#include <systemc.h>
#include <iostream>
#include <iomanip>
#include <string.h>
#include <fstream> 
#include "aca2009.h"

using namespace std;

int ProbeWrites = 0;
int ProbeReads = 0;
int ProbeFlushes = 0;
int ProbeUpgrades = 0;

#define CACHE_SETS 8
#define CACHE_LINES 128

class Bus_if : public virtual sc_interface
{

	public:
		virtual bool read(int writer, int address) = 0;
		virtual bool write(int writer, int address, int data) = 0;
		virtual bool readx(int writer, int address, int data) = 0;
		virtual bool upgr(int writer, int address) = 0;
		virtual bool flush(int writer, int receiver, int address, int data) = 0;
};



#if 0
typedef	struct 
{
	bool valid; //can be deleted later
	State *current; //added to indicate its state
	sc_uint<20> tag;
	int data[8]; //8 words = 32 byte line size 
} aca_cache_line;
#endif 

class Cache;

/* Base class for States which have dummy implementations for all 
 * the CPU/Bus requests of the MOESI protocol
 */

class State
{
	public:
		enum STATE_TYPE 
		{
			STATE_MODIFIED,
			STATE_OWNED,
			STATE_EXCLUSIVE,
			STATE_SHARED,
			STATE_INVALID,
		};	

		unsigned int state_type;

		virtual void processorRd(Cache *c, aca_cache_line *c_line, int addr)
		{
			//do nothing
		}

		virtual void processorWr(Cache *c, aca_cache_line *c_line, int addr, int data)
		{
			//do nothing
		}

		virtual void snoopedBusRd(Cache *c, aca_cache_line *c_line, int addr, int requester){
			//do nothing 
		}

		virtual void snoopedBusRdX(Cache *c, aca_cache_line *c_line, int addr, int requester){
			//do nothing
		}

		virtual void snoopedBusUpgr(Cache *c, aca_cache_line *c_line, int addr){
			//do nothing 
		}

		void isShared(Cache *c, aca_cache_line *c_line, int addr){
			//do nothing 
		}

		void notShared(Cache *c, aca_cache_line *c_line, int addr){
			//do nothing 
		}
	

};

// Override requests pertaining to MODIFIED state

class Modified : public State
{
	public:
		Modified()
		{
			state_type = STATE_MODIFIED;
		}

		// Override snoopedBusRd to perform Flush and change state to Owned
		void snoopedBusRd(Cache *c, aca_cache_line *c_line, int addr, int requester);

		// Override snoopedBusRdX to perform Flush and change state to Invalid
		void snoopedBusRdX(Cache *c, aca_cache_line *c_line,int addr, int requester);

};
// Override requests pertaining to OWNED state

class Owned : public State
{
	public:
		Owned()
		{
			state_type = STATE_OWNED;
		}

		// Override processorWr to perform BusUpgr change state to Modified
		void processorWr(Cache *c, aca_cache_line *c_line, int addr, int data);

		// Override snoopedBusRdX to perform Flush and change state to Invalid
		void snoopedBusRdX(Cache *c, aca_cache_line *c_line, int addr, int requester);

		// Override snoopedBusUpgr and change state to Invalid
		void snoopedBusUpgr(Cache *c, aca_cache_line *c_line, int addr);
};

// Override requests pertaining to EXCLUSIVE state

class Exclusive : public State
{
	public:
		Exclusive()
		{
			state_type = STATE_EXCLUSIVE;
		}


		// Override processorWr to change state to Modified
		void processorWr(Cache *c, aca_cache_line *c_line, int addr, int data);

		// Override snoopedBusRd to perform Flush and change state to Owned
		void snoopedBusRd(Cache *c, aca_cache_line *c_line, int addr, int requester);

		// Override snoopedBusRdX to perform Flush and change state to Invalid
		void snoopedBusRdX(Cache *c, aca_cache_line *c_line,int addr, int requester);


};

// Override requests pertaining to SHARED state

class Shared : public State
{
	public:
		Shared()
		{
			state_type = STATE_SHARED;
		}
		// Override processorWr to perform BusUpgr change state to Modified
		void processorWr(Cache *c, aca_cache_line *c_line, int addr, int data);

		// Override snoopedBusRdX to change state to Invalid
		void snoopedBusRdX(Cache *c, aca_cache_line *c_line, int addr, int requester);

		// Override snoopedBusUpgr to change state to Invalid
		void snoopedBusUpgr(Cache *c, aca_cache_line *c_line, int addr);

};

// Override requests pertaining to INVALID state

class Invalid : public State
{
	public:

		Invalid()
		{
			state_type = STATE_INVALID;
		}

		// Override processorRd to perform BusRd
		void processorRd(Cache *c, aca_cache_line *c_line, int addr);

		// Override processorWr to perform BusRdX  
		void processorWr(Cache *c, aca_cache_line *c_line, int addr, int data);

		void isShared(Cache *c, aca_cache_line *c_line, int addr);

		void notShared(Cache *c, aca_cache_line *c_line, int addr);

};

class aca_cache_line
{
	public:
		State *current; //added to indicate its state
		//bool valid; //can be deleted later
		sc_uint<20> tag;
		int data[8]; //8 words = 32 byte line size 

		aca_cache_line()
		{
			setCurrent(new Invalid);
		}

		void setCurrent(State *s)
		{
			current = s;
		}

		State* getCurrent()
		{
			return current;
		}

};

typedef	struct 
{
	aca_cache_line cache_line[CACHE_LINES];
} aca_cache_set; 

typedef	struct
{
	aca_cache_set cache_set[CACHE_SETS];
} aca_cache;



SC_MODULE(Cache) 
{

	public:
		enum BUS_REQ 
		{
			BUS_RD,
			BUS_WR,
			BUS_RDX,//-> seems have same response with BUS_WR
			BUS_FLUSH,
			BUS_UPGR,
			BUS_INVALID,
		};

		enum Function 
		{
			FUNC_READ,
			FUNC_WRITE
		};

		enum RetCode 
		{
			RET_READ_DONE,
			RET_WRITE_DONE,
		};

		sc_in<bool>     Port_CLK;
		sc_in<Function> Port_Func;
		sc_in<int>      Port_Addr;
		sc_out<RetCode> Port_Done;
		sc_inout_rv<32> Port_Data;

		sc_out<bool> 	Port_Hit;

		// Input ports for snooping the Bus
		sc_in_rv<32> 	Port_BusWriter;
		sc_in_rv<32> 	Port_BusAddr;
		sc_in_rv<32> 	Port_BusReq;
		sc_in_rv<32>    Port_BusData;
		sc_in_rv<32>    Port_Receiver;

		// Interface port to issue commands to the Bus
		sc_port<Bus_if>	Port_Bus;

		int cache_id;	
		int snooping;

		bool shared;

		SC_CTOR(Cache) 
		{
			// Main process
			SC_THREAD(execute);
			sensitive << Port_CLK.pos();
			dont_initialize();

			// Snooping process
			SC_THREAD(snoop);
			sensitive << Port_CLK.pos();
			dont_initialize();

			cache = new aca_cache;
			lru_table= new unsigned char[CACHE_LINES];
			for (int i = 0; i<8; i++)
				valid_lines[i] = false;
			for (int j = 0; j< CACHE_LINES; j++)
				lru_table[j] = 0;

			shared = false;
#if 0			
			//initiallize all the cache lines to Invalid			
			for (int i = 0; i< CACHE_LINES; i++)
				for (int j = 0; j< CACHE_SETS; j++)
					cache->cache_set[j].cache_line[i].current = new Invalid();
#endif
		}
		~Cache() 
		{
			delete cache;
			delete lru_table;

		}

		aca_cache_line* getCacheLine(int addr)
		{
			aca_cache_line *c_line;
			sc_uint<20> tag = 0;
			unsigned int line_index;
			line_index = (addr & 0x00000FE0) >> 5;
			tag = addr >> 12;

			for ( int i=0; i <CACHE_SETS; i++ ){
				c_line = &(cache->cache_set[i].cache_line[line_index]);
				// If state is anything else other than INVALID, consider them as Valid

				State *cur_state = c_line -> getCurrent();
				if (cur_state -> state_type != State::STATE_INVALID){
					valid_lines[i] = true;
					if ( c_line -> tag == tag){
						return c_line;
					}

				}
				else{
					valid_lines[i] = false;
				}
			}
			return NULL;	

		}
#if 0			

		aca_cache_line* getCacheLine(int addr)
		{
			aca_cache_line *c_line;
			sc_uint<20> tag = 0;
			unsigned int line_index;
			line_index = (addr & 0x00000FE0) >> 5;
			tag = addr >> 12;

			for ( int i=0; i <CACHE_SETS; i++ ){
				c_line = &(cache->cache_set[i].cache_line[line_index]);
				if (c_line->tag == tag)
					return c_line;
			}

			return NULL;	
		}
#endif
		State* getCacheState(int addr)
		{

			aca_cache_line *c_line;
			c_line = getCacheLine(addr);
			if (c_line != NULL)
				return c_line->current;
			return NULL;	
		}



	private:

		aca_cache *cache;
		unsigned char *lru_table;
		bool valid_lines[8];

		void processorRd(aca_cache_line *c_line, int addr)
		{
			c_line -> getCurrent() -> processorRd(this, c_line, addr);
		}

		void processorWr(aca_cache_line *c_line, int addr, int data)
		{
			c_line -> getCurrent() ->processorWr(this, c_line, addr, data);
		}

		void snoopedBusRd(int addr, int requester)
		{
			aca_cache_line c_line =  getCacheLine(addr);
			if (c_line != NULL)
				c_line -> getCurrent()->snoopedBusRd(this, c_line, addr, requester);
		}

		void snoopedBusRdX(int addr, int requester)
		{
			aca_cache_line c_line =  getCacheLine(addr);
			if (current != NULL)
				c_line -> getCurrent()->snoopedBusRdX(this, c_line, addr, requester);
		}

		void snoopedBusUpgr(int addr)
		{
			aca_cache_line c_line =  getCacheLine(addr);
			if (current != NULL)
				c_line -> getCurrent() ->snoopedBusUpgr(this, c_line, addr);
		}

		char *binary (unsigned char v) { 
			static char binstr[9] ; 
			int i ; 

			binstr[8] = '\0' ; 
			for (i=0; i<8; i++) { 
				binstr[7-i] = v & 1 ? '1' : '0' ; 
				v = v / 2 ; 
			} 

			return binstr ; 
		} 

		void global_mem_read_access(aca_cache_line *c_line)
		{
			for (int i = 0; i< 8; i++){//here I think can be interrupted by someone else's flush
				if(!shared)
				{
					wait(100); //fetch 8 * words data from memory to cache
					c_line -> data[i] = rand()%10000; //load the memory line(32bytes) to the appropriate cache line
				}
				else
				{
					/* Data supplied by another cache which flushes */
					break;
				}
			}
			/* Change state accordingly */
			if(!shared)
			{
				c_line -> getCurrent() -> isShared(this, c_line);	
			}
			else
			{
				c_line -> getCurrent() -> notShared(this, c_line);	

			}
			shared = false;

		}

		void global_mem_write_access(aca_cache_line *c_line)
		{
			for (int i = 0; i< 8; i++){//here I think can be interrupted by someone else's flush
				if(!shared)
				{
					wait(100); //fetch 8 * words data from memory to cache
					c_line -> data[i] = rand()%10000; //load the memory line(32bytes) to the appropriate cache line
				}
				else
				{
					/* Data supplied by another cache which flushes */
					break;
				}
			}
			/* Change state to Modified */
			shared = false;

		}

		void snoop();

		aca_cache_line* updateLRU(int addr, bool *hit_check);


		void execute() 
		{
			while (true)
			{
				wait(Port_Func.value_changed_event());

				Function f = Port_Func.read();
				int addr   = Port_Addr.read();

				aca_cache_line *c_line;
				sc_uint<20> tag = 0;
				unsigned int line_index;
				unsigned int word_index = 0;
				bool hit = false;

				//determine whether a hit
				cout << "addr: " << hex << addr << endl;
				line_index = (addr & 0x00000FE0) >> 5;
				tag = addr >> 12;
				cout << "line_index: " << line_index <<  " tag: " <<tag << endl;
				word_index = ( addr & 0x0000001C ) >> 2;

#ifdef MASK

				cout << "before replacing--------------" <<endl;
				cout<<"lru_table: "<<binary(lru_table[line_index])<<endl;
				cout <<setw(8) << "set"<< setw(8) <<  "valid" << setw(8) <<  "tag" <<endl;
#endif
				for (int set = 0; set < 8; set++){ 
					c_line = &(cache->cache_set[set].cache_line[line_index]);
#ifdef MASK
					cout <<setw(8)<<  set <<setw(8) << c_line -> valid << setw(8)<< c_line -> tag <<endl; 
#endif
				}

				c_line = updateLRU(addr,&hit);

				if (f == FUNC_WRITE) 
				{
					int cpu_data = Port_Data.read().to_int();

					cout << sc_time_stamp() << ": MEM received write" << endl;

					processorWr(c_line,addr,cpu_data);

					if (hit){ //write hit

						Port_Hit.write(true);
						stats_writehit(cache_id);

						c_line -> data[word_index] = cpu_data;
						wait();//consume 1 cycle
						cout << sc_time_stamp() << ": Cache write hit!" << endl;
					}
					else //write miss
					{
						Port_Hit.write(false);
						stats_writemiss(cache_id);
						cout << sc_time_stamp() << ": Cache write miss!" << endl;

						c_line -> tag = tag;
#if 0
						// Write allocate
						for (int i = 0; i< 8; i++){//here I think can be interrupted by someone else's flush
							wait(100); //fetch 8 * words data from memory to cache
							c_line -> data[i] = rand()%10000; //load the memory line(32bytes) to the appropriate cache line
						}
#endif

						global_mem_write_access(c_line);

						c_line -> data[word_index] =  cpu_data; //actual write from processor to cache line
					}

					Port_Done.write( RET_WRITE_DONE );
				}
				else//a read comes to cache
				{
					cout << sc_time_stamp() << ": MEM received read" << endl;

					processorRd(c_line,addr);

					if (hit){ //read hit
						Port_Hit.write(true);
						stats_readhit(cache_id);

						Port_Data.write( c_line -> data[word_index] );
						cout << sc_time_stamp() << ": Cache read hit!" << endl;
					}
					else //read miss
					{
						Port_Hit.write(false);
						stats_readmiss(cache_id);
						cout << sc_time_stamp() << ": Cache read miss!" << endl;

						c_line -> tag = tag;

#if 0
						// write allocate
						for (int i = 0; i< 8; i++){//here I think can be interrupted by someone else's flush
							wait(100); //fetch 8 * words data from memory to cache
							c_line -> data[i] = rand()%10000; //load the memory line(32bytes) to the appropriate cache line
						}
#endif

						global_mem_read_access(c_line);

						Port_Data.write(c_line -> data[word_index]); //return data to the CPU


					}

					Port_Done.write( RET_READ_DONE );
					wait();
					Port_Data.write("ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ");
				}
#ifdef MASK
				//at here means a read or a write has happened
				cout << "after replacing----------------------" <<endl;
				cout <<"lru_table: "<<binary(lru_table[line_index])<<endl;
				//cout <<"use invalid line in set: "<<
				cout <<setw(8) << "set"<< setw(8) <<  "valid" << setw(8) <<  "tag" <<endl;
#endif
				for (int set = 0; set < 8; set++){
					c_line = &(cache->cache_set[set].cache_line[line_index]);
#ifdef MASK
					cout <<setw(8)<<  set <<setw(8) << c_line -> valid << setw(8)<< c_line -> tag <<endl; 
#endif
				}
				cout<<endl;
			}
		}
};

// Modified state overrided function implementations

void Modified :: snoopedBusRd(Cache *c, aca_cache_line *c_line, int addr, int requester)
{
	//flush
	c->Port_Bus->flush(c->cache_id, requester, addr, rand()%44);//assume just random data as it is not concerned for this lab

	// BUS_RD in Exclusive state changes state to Owned 
	c_line -> setCurrent(new Owned);
	delete this;
}

void Modified :: snoopedBusRdX(Cache *c, aca_cache_line *c_line,int addr, int requester)
{
	//flush
	c->Port_Bus->flush(c->cache_id, requester, addr, rand()%44);//assume just random data as it is not concerned for this lab

	// BUS_RDX in Exclusive state changes state to Invalid
	c_line -> setCurrent(new Invalid);
	delete this;
}

// Owned state overrided function implementations

void Owned :: processorWr(Cache *c, aca_cache_line *c_line, int addr, int data)
{
	// Issue a BusUpgr on the bus
	c->Port_Bus->upgr(c->cache_id,addr);

	// Change state from Owned to Modified
	c_line -> setCurrent(new Modified);
	delete this;
}

void Owned :: snoopedBusRdX(Cache *c, aca_cache_line *c_line, int addr, int requester)
{
	//flush
	c->Port_Bus->flush(c->cache_id, requester, addr, rand()%44);//assume just random data as it is not concerned for this lab

	// BUS_RDX in Exclusive state changes state to Invalid
	c_line -> setCurrent(new Invalid);
	delete this;
}

void Owned :: snoopedBusUpgr(Cache *c, aca_cache_line *c_line, int addr)
{
	// BUS_RDX in Exclusive state changes state to Invalid
	c_line -> setCurrent(new Invalid);
	delete this;
}

// Exclusive state overrided function implementation
void Exclusive :: processorWr(Cache *c, aca_cache_line *c_line, int addr, int data)
{
	// Change state from Exclusive to Modified
	c_line -> setCurrent(new Modified);
	delete this;
}

void Exclusive :: snoopedBusRd(Cache *c, aca_cache_line *c_line,int addr,int requester)
{
	//flush
	c->Port_Bus->flush(c->cache_id, requester, addr, rand()%44);//assume just random data as it is not concerned for this lab

	// BUS_RD in Exclusive state changes state to Owned 
	c_line -> setCurrent(new Owned);
	delete this;
}

void Exclusive :: snoopedBusRdX(Cache *c, aca_cache_line *c_line,int addr,int requester)
{
	//flush
	c->Port_Bus->flush(c->cache_id, requester, addr, rand()%44);//assume just random data as it is not concerned for this lab

	// BUS_RDX in Exclusive state changes state to Invalid
	c_line -> setCurrent(new Invalid);
	delete this;
}

// Invalid state overrided function implementation

void Shared :: processorWr(Cache *c, aca_cache_line *c_line, int addr, int data)
{
	// Issue a BusUpgr on the bus
	c->Port_Bus->upgr(c->cache_id,addr);

	// PR_WR in Shared state changes state to Modified
	c_line -> setCurrent(new Modified);
	delete this;
}

void Shared :: snoopedBusRdX(Cache *c, aca_cache_line *c_line, int addr, int requester)
{
	// BUS_RDX in Shared state changes state to Invalid
	c_line -> setCurrent(new Invalid);
	delete this;
}

void Shared :: snoopedBusUpgr(Cache *c, aca_cache_line *c_line, int addr)
{
	// BUS_RDX in Exclusive state changes state to Invalid
	c_line -> setCurrent(new Invalid);
	delete this;
}

// Invalid state overrided function implementation

void Invalid :: processorRd(Cache *c, aca_cache_line *c_line, int addr) 
{
	//Issue a BusRd on the bus
	c->Port_Bus->read(c->cache_id, addr);
}

void Invalid :: processorWr(Cache *c, aca_cache_line *c_line, int addr, int data) 
{
	//Issue a BusRdX on the bus
	c->Port_Bus->readx(c->cache_id, addr, data);
	c_line -> setCurrent(new Modified);
	delete this;
}


void Invalid :: isShared(Cache *c, aca_cache_line *c_line) 
{
	c_line -> setCurrent(new Shared);
	delete this;
}

void Invalid :: notShared(Cache *c, aca_cache_line *c_line) 
{
	c_line -> setCurrent(new Exclusive);
	delete this;
}

void Cache::snoop()
{
	while (true)
	{
		// Snoop for requests on Bus
		wait(Port_BusReq.value_changed_event());
		int writer = Port_BusWriter.read().to_int();
		int data = Port_BusData.read().to_int();
		int flush_recepient;

		// If the writer on the bus is not myself, probe the bus
		if(writer != cache_id){
			// Fetch the address on the bus to check its local cache
			int addr= Port_BusAddr.read().to_int();
			int req = Port_BusReq.read().to_int();
#if 0
			aca_cache_line *c_line;
			sc_uint<20> tag = 0;
			unsigned int line_index;
			line_index = (addr & 0x00000FE0) >> 5;
			tag = addr >> 12;
#endif

			switch(req)
			{
				case BUS_RD:
					ProbeReads++;
					snoopedBusRd(addr,writer);	
					break;

				case BUS_RDX:

				case BUS_WR:
					ProbeWrites++;
					snoopedBusRdX(addr,writer);
					break;

				case BUS_FLUSH:
					/*snooper detect that somebody flushes the thing I need */
					ProbeFlushes++;

					/* Check if the flush is intended for my cache */
					flush_recepient = Port_Receiver.read().to_int();
					if(flush_recepient == cache_id)
					{
						/* Signal the execute thread to break the wait */
						shared = true;
					}
					break;

				case BUS_UPGR:
					ProbeUpgrades++;

					/* Make the cache line Invalid */
					snoopedBusUpgr(addr);
					break;

				default://BUS_INVALID command
					cout<<"a invalid command was issued in the end"<<endl;
					break;
			}
		}
		wait();
	}

}

aca_cache_line* Cache::updateLRU(int addr, bool *hit_check)
{

	aca_cache_line *c_line;
	sc_uint<20> tag = 0;
	unsigned int line_index;
	line_index = (addr & 0x00000FE0) >> 5;
	tag = addr >> 12;
	//bool hit   = false;
	int hit_set = -1;

	line_index = (addr & 0x00000FE0) >> 5;
	tag = addr >> 12;

	for ( int i=0; i <CACHE_SETS; i++ ){
		c_line = &(cache->cache_set[i].cache_line[line_index]);
		// If state is anything else other than INVALID, consider them as Valid

		State *cur_state = c_line -> getCurrent();
		if (cur_state -> state_type != State::STATE_INVALID){
			valid_lines[i] = true;
			if ( c_line -> tag == tag){
				*hit_check = true;
				hit_set=i; 
			}

		}
		else{
			valid_lines[i] = false;
		}
	}

	if(*hit_check){
		switch (hit_set)
		{	
			case 0: lru_table[line_index] |=0b1101000; break;
			case 1: lru_table[line_index]  = (lru_table[line_index]  & 0b0010111) | 0b1100000;break;
			case 2: lru_table[line_index]  = (lru_table[line_index]  & 0b0011011) | 0b1000100;break;
			case 3: lru_table[line_index]  = (lru_table[line_index]  & 0b0011011) | 0b1000000;break;
			case 4: lru_table[line_index]  = (lru_table[line_index]  & 0b0101101) | 0b0010010;break;
			case 5: lru_table[line_index]  = (lru_table[line_index]  & 0b0101101) | 0b0010000;break;
			case 6: lru_table[line_index]  = (lru_table[line_index]  & 0b0101110) | 0b0000001;break;
			case 7: lru_table[line_index]  = (lru_table[line_index]  & 0b0101110);break;
			default: cout << "Damn here !!!!" << endl;
		}
	}
	else{
		for ( int i=0; i <CACHE_SETS; i++ ){
			if (valid_lines[i] == false){ //use an invalid line
				// write allocate
				c_line = &(cache->cache_set[i].cache_line[line_index]);
				//update the lru table after the cache update
				switch (i)
				{
					case 0: lru_table[line_index] |=0b1101000; break;
					case 1: lru_table[line_index]  = (lru_table[line_index]  & 0b0010111) | 0b1100000;break;
					case 2: lru_table[line_index]  = (lru_table[line_index]  & 0b0011011) | 0b1000100;break;
					case 3: lru_table[line_index]  = (lru_table[line_index]  & 0b0011011) | 0b1000000;break;
					case 4: lru_table[line_index]  = (lru_table[line_index]  & 0b0101101) | 0b0010010;break;
					case 5: lru_table[line_index]  = (lru_table[line_index]  & 0b0101101) | 0b0010000;break;
					case 6: lru_table[line_index]  = (lru_table[line_index]  & 0b0101110) | 0b0000001;break;
					case 7: lru_table[line_index]  = (lru_table[line_index]  & 0b0101110);break;

				}
				break;

			}
			else if(i == CACHE_SETS-1){// all lines are valid
				//lru
				//write back the previous line to mem and replace the lru line 
				int set_index_toreplace = 8;


				if ( (lru_table[line_index]  & 0b1101000) == 0 ){ //find the cache line to replace
					c_line = &(cache->cache_set[0].cache_line[line_index]);
					set_index_toreplace = 0;
				}
				else if ( (lru_table[line_index]  & 0b1101000) == 0b0001000 ) {
					c_line = &(cache->cache_set[1].cache_line[line_index]);
					set_index_toreplace = 1;
				}
				else if ( (lru_table[line_index]  & 0b1100100) == 0b0100000 ) {
					c_line = &(cache->cache_set[2].cache_line[line_index]);
					set_index_toreplace = 2;
				}
				else if ( (lru_table[line_index]  & 0b1100100) == 0b0100100 ) {
					c_line = &(cache->cache_set[3].cache_line[line_index]);
					set_index_toreplace = 3;
				}
				else if ( (lru_table[line_index]  & 0b1010010) == 0b1000000 ) {
					c_line = &(cache->cache_set[4].cache_line[line_index]);
					set_index_toreplace = 4;
				}
				else if ( (lru_table[line_index]  & 0b1010010) == 0b1000010 ) {
					c_line = &(cache->cache_set[5].cache_line[line_index]);
					set_index_toreplace = 5;
				}
				else if ( (lru_table[line_index]  & 0b1010001) == 0b1010000 ) {
					c_line = &(cache->cache_set[6].cache_line[line_index]);
					set_index_toreplace = 6;
				}
				else if ( (lru_table[line_index]  & 0b1010001) == 0b1010001 ) {
					c_line = &(cache->cache_set[7].cache_line[line_index]);
					set_index_toreplace = 7;
				}
#if 1
				// write back
				for(int i=0; i<8;i++)//write the cache line back to the memory
					wait(100);
#endif
				switch (set_index_toreplace)
				{
					case 0: lru_table[line_index] |=0b1101000; break;
					case 1: lru_table[line_index]  = (lru_table[line_index]  & 0b0010111) | 0b1100000;break;
					case 2: lru_table[line_index]  = (lru_table[line_index]  & 0b0011011) | 0b1000100;break;
					case 3: lru_table[line_index]  = (lru_table[line_index]  & 0b0011011) | 0b1000000;break;
					case 4: lru_table[line_index]  = (lru_table[line_index]  & 0b0101101) | 0b0010010;break;
					case 5: lru_table[line_index]  = (lru_table[line_index]  & 0b0101101) | 0b0010000;break;
					case 6: lru_table[line_index]  = (lru_table[line_index]  & 0b0101110) | 0b0000001;break;
					case 7: lru_table[line_index]  = (lru_table[line_index]  & 0b0101110);break;
					default: cout << "Buggy here !!!!! should not come here" <<endl; 

				}

			}
		}
	}
	return c_line;

}

class Bus : public Bus_if,public sc_module
{

	public:
		// ports
		sc_in<bool> Port_CLK;

		// Write ports in the bus used by the Caches
		sc_signal_rv<32> Port_BusReq;
		sc_signal_rv<32> Port_BusWriter;
		sc_signal_rv<32> Port_BusAddr;
		sc_signal_rv<32> Port_BusData;
		sc_signal_rv<32> Port_BusReceiver;

		// SystemC Mutex to provide atomicity
		sc_mutex bus;

		long waits;
		long reads;
		long writes;
		long flushes;
	public:
		SC_CTOR(Bus)
		{
			// Handle Port_CLK to simulate delay
			sensitive << Port_CLK.pos();

			// Initialize some bus properties
			Port_BusAddr.write("ZZZZZZZZZZZZZZZZZZZZZ");
			Port_BusReq.write("ZZZZZZZZZZZZZZZZZZZZZ");
			Port_BusWriter.write("ZZZZZZZZZZZZZZZZZZZZZ");
			Port_BusReceiver.write("ZZZZZZZZZZZZZZZZZZZZZ");

			waits = 0;
			reads = 0;
			writes = 0; 
			flushes = 0;
		}

		virtual bool read(int writer, int addr)
		{
			//Try to acquire the bus
			while(bus.trylock() == -1){
				//Retry after 1 cycle
				waits++;
				wait();
			}
			reads++;

			Port_BusAddr.write(addr);
			Port_BusWriter.write(writer);
			Port_BusReq.write(Cache::BUS_RD);

			//wait for everyone to revieve
			wait();

			// Pull ports to tristate for future writes
			Port_BusReq.write("ZZZZZZZZZZZZZZZZZZZZZ");
			Port_BusAddr.write("ZZZZZZZZZZZZZZZZZZZZZ");
			Port_BusWriter.write("ZZZZZZZZZZZZZZZZZZZZZ");

			// Release the lock
			bus.unlock();

			return true;

		}

		virtual bool write(int writer, int addr, int data) 
		{
			//Try to acquire the bus
			while(bus.trylock() == -1){
				//Retry after 1 cycle
				waits++;
				wait();
			}

			writes++;

			Port_BusAddr.write(addr);
			Port_BusWriter.write(writer);
			Port_BusReq.write(Cache::BUS_WR);

			//wait for everyone to revieve
			wait();

			// Pull ports to tristate for future writes
			Port_BusReq.write("ZZZZZZZZZZZZZZZZZZZZZ");
			Port_BusAddr.write("ZZZZZZZZZZZZZZZZZZZZZ");
			Port_BusWriter.write("ZZZZZZZZZZZZZZZZZZZZZ");

			// Release the lock
			bus.unlock();

			return true;
		}

		virtual bool readx(int writer, int addr, int data) 
		{
			//Try to acquire the bus
			while(bus.trylock() == -1){
				//Retry after 1 cycle
				waits++;
				wait();
			}

			writes++;

			Port_BusAddr.write(addr);
			Port_BusReq.write(Cache::BUS_RDX);
			Port_BusWriter.write(writer);

			//wait for everyone to revieve
			wait();

			// Pull ports to tristate for future writes
			Port_BusReq.write("ZZZZZZZZZZZZZZZZZZZZZ");
			Port_BusAddr.write("ZZZZZZZZZZZZZZZZZZZZZ");
			Port_BusWriter.write("ZZZZZZZZZZZZZZZZZZZZZ");

			// Release the lock
			bus.unlock();

			return true;
		}

		virtual bool upgr(int writer, int addr) 
		{
			//Try to acquire the bus
			while(bus.trylock() == -1){
				//Retry after 1 cycle
				waits++;
				wait();
			}

			writes++;

			Port_BusAddr.write(addr);
			Port_BusReq.write(Cache::BUS_UPGR);
			Port_BusWriter.write(writer);

			//wait for everyone to revieve
			wait();

			// Pull ports to tristate for future writes
			Port_BusReq.write("ZZZZZZZZZZZZZZZZZZZZZ");
			Port_BusAddr.write("ZZZZZZZZZZZZZZZZZZZZZ");
			Port_BusWriter.write("ZZZZZZZZZZZZZZZZZZZZZ");

			// Release the lock
			bus.unlock();

			return true;
		}

		virtual bool flush(int writer, int receiver, int addr, int data) 
		{
			//Try to acquire the bus
			while(bus.trylock() == -1){
				//Retry after 1 cycle
				waits++;
				wait();
			}

			flushes++;
			Port_BusAddr.write(addr);
			Port_BusData.write(data);
			Port_BusReq.write(Cache::BUS_FLUSH);
			Port_BusWriter.write(writer);
			Port_BusReceiver.write(receiver);

			//wait for everyone to revieve
			wait();

			// Pull ports to tristate for future writes
			Port_BusReq.write("ZZZZZZZZZZZZZZZZZZZZZ");
			Port_BusAddr.write("ZZZZZZZZZZZZZZZZZZZZZ");
			Port_BusData.write("ZZZZZZZZZZZZZZZZZZZZZ");
			Port_BusWriter.write("ZZZZZZZZZZZZZZZZZZZZZ");

			// Release the lock
			bus.unlock();
			return true;
		}





};

SC_MODULE(CPU) 
{

	public:
		sc_in<bool>             Port_CLK;
		sc_in<Cache::RetCode>   Port_MemDone;
		sc_out<Cache::Function> Port_MemFunc;
		sc_out<int>             Port_MemAddr;
		sc_inout_rv<32>         Port_MemData;
		int cpu_id;

		SC_CTOR(CPU) 
		{
			SC_THREAD(execute);
			sensitive << Port_CLK.pos();
			dont_initialize();
		}

	private:
		void execute() 
		{
			TraceFile::Entry    tr_data;
			Cache::Function  f;

			// Loop until end of tracefile
			while(!tracefile_ptr->eof())
			{
				// Get the next action for the processor in the trace
				if(!tracefile_ptr->next(cpu_id, tr_data))
				{
					cerr << "Error reading trace for CPU" << endl;
					break;
				}

				// To demonstrate the statistic functions, we generate a 50%
				// probability of a 'hit' or 'miss', and call the statistic
				// functions below
				//int j = rand()%2;

				switch(tr_data.type)
				{
					case TraceFile::ENTRY_TYPE_READ:
						f = Cache::FUNC_READ;
						/*
						   if(j)
						   stats_readhit(0);
						   else
						   stats_readmiss(0);
						 */
						break;

					case TraceFile::ENTRY_TYPE_WRITE:
						f = Cache::FUNC_WRITE;
						/*
						   if(j)
						   stats_writehit(0);
						   else
						   stats_writemiss(0);
						 */						
						break;

					case TraceFile::ENTRY_TYPE_NOP:
						break;

					default:
						cerr << "Error, got invalid data from Trace" << endl;
						exit(0);
				}

				if(tr_data.type != TraceFile::ENTRY_TYPE_NOP)
				{
					Port_MemAddr.write(tr_data.addr);

					Port_MemFunc.write(f);
					if (f == Cache::FUNC_WRITE) 
					{
						cout << sc_time_stamp() << ": CPU "<<cpu_id<<" sends write" << endl;

						uint32_t data = rand();
						Port_MemData.write(data);
						wait(); //this waiting for 1 cycle is mapping to the one cycle wait in the cache write hit.
						Port_MemData.write("ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ");
					}
					else
					{
						cout << sc_time_stamp() << ": CPU  "<<cpu_id<<"sends read" << endl;
					}
					//cout <<"CPU: "<<"waiting for cache response" <<endl;
					wait(Port_MemDone.value_changed_event());
					//cout <<"CPU: "<<"get cache response" <<endl;

					if (f == Cache::FUNC_READ)
					{
						cout << sc_time_stamp() << ": CPU reads: " << Port_MemData.read() << endl;
					}
				}
				else
				{
					cout << sc_time_stamp() << ": CPU executes NOP" << endl;
				}
				// Advance one cycle in simulated time            
				wait();
			}

			// Finished the Tracefile, now stop the simulation
			sc_stop();
		}
};


int sc_main(int argc, char* argv[])
{
	try
	{
		// Get the tracefile argument and create Tracefile object
		// This function sets tracefile_ptr and num_cpus
		init_tracefile(&argc, &argv);

		// Initialize statistics counters
		stats_init();

		// Global clock signal
		sc_clock clk;

		Bus bus("bus");

		// Connect the global clock to Bus module clock
		bus.Port_CLK(clk);

		// Ports for Cache/CPU connection
		sc_buffer<Cache::Function>  sigMemFunc[num_cpus];
		sc_signal<int>              sigMemAddr[num_cpus];
		sc_signal_rv<32>            sigMemData[num_cpus];
		sc_buffer<Cache::RetCode>   sigMemDone[num_cpus];
		sc_signal<bool> 	    sigMemHit[num_cpus];

		Cache *cache[num_cpus];
		CPU   *cpu[num_cpus];

		for(unsigned int i = 0; i < num_cpus; i++)
		{
			char name_cache[12];
			char name_cpu[12];

			sprintf(name_cache, "cache_%d", i);
			sprintf(name_cpu, "cpu_%d", i);

			/* Create objects for Cache and CPU */	
			cache[i] = new Cache(name_cache);
			cpu[i] = new CPU(name_cpu);

			/* Set IDs */
			cpu[i]->cpu_id = i;
			cache[i]->cache_id = i;

			/* Connect Cache to Bus */
			cache[i]->Port_BusAddr(bus.Port_BusAddr);	
			cache[i]->Port_BusWriter(bus.Port_BusWriter);	
			cache[i]->Port_BusReq(bus.Port_BusReq);	
			cache[i]->Port_BusData(bus.Port_BusData);	
			cache[i]->Port_Receiver(bus.Port_BusReceiver);	

			cache[i]->Port_Bus(bus);

			/* Connect Cache to CPU */
			cache[i]->Port_Func(sigMemFunc[i]);	
			cache[i]->Port_Addr(sigMemAddr[i]);	
			cache[i]->Port_Data(sigMemData[i]);	
			cache[i]->Port_Done(sigMemDone[i]);	
			cache[i]->Port_Hit(sigMemHit[i]);

			/* Connect CPU to Cache */
			cpu[i]->Port_MemFunc(sigMemFunc[i]);	
			cpu[i]->Port_MemAddr(sigMemAddr[i]);	
			cpu[i]->Port_MemData(sigMemData[i]);	
			cpu[i]->Port_MemDone(sigMemDone[i]);	

			/* Connect clocks */
			cache[i]->Port_CLK(clk);
			cpu[i]->Port_CLK(clk);
		}

		cout << "Running (press CTRL+C to interrupt)... " << endl;

		sc_trace_file *wf = sc_create_vcd_trace_file("CPU_MEM");

		// Dump the clock signal
		sc_trace(wf, clk, "clock");

		for(unsigned int i=0; i<num_cpus; i++)
		{
			char addr_cpu[16];
			char data_cpu[16];
			char hit_cache[16];

			sprintf(addr_cpu, "cpu_addr_%d", i);
			sprintf(data_cpu, "cpu_data_%d", i);
			sprintf(hit_cache, "cache_hit_%d", i);

			// Dump traces of each CPU and Cache signals
			sc_trace(wf, sigMemAddr[i], addr_cpu);
			sc_trace(wf, sigMemData[i], data_cpu);
			sc_trace(wf, sigMemHit[i], hit_cache);
		}

		// Dump traces of Bus
		sc_trace(wf, bus.Port_BusAddr , "addr_on_bus");
		sc_trace(wf, bus.Port_BusWriter, "writer_on_bus");
		sc_trace(wf, bus.Port_BusReq, "req_on_bus");

		// Start Simulation
		sc_start();

		// Print statistics after simulation finished

		stats_print();
		cout<<endl;

		printf("CPU\tProbeReads\tProbeWrites\n");

		for(unsigned int i =0; i < num_cpus; i++)
		{

			printf("%d\t%d\t%d\n", i, ProbeReads,ProbeWrites);
		}
		cout<<endl;

		printf("waits\treads\ttotal_access(r+w)\twait_per_access\n");
		long total_accesses = bus.reads+bus.writes;

		printf("%ld\t%ld\t%ld\t%ld\t%f\n",bus.waits, bus.reads, bus.writes, total_accesses,(double)((float)bus.waits/(float)total_accesses));

	}
	catch (exception& e)
	{
		cerr << e.what() << endl;
	}

	return 0;
}
