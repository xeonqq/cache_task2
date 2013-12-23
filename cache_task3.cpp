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

#define CACHE_SETS 8
#define CACHE_LINES 128

class Bus_if : public virtual sc_interface
{

	public:
		virtual bool read(int writer, int address) = 0;
		virtual bool write(int writer, int address, int data) = 0;
		virtual bool readx(int writer, int address, int data) = 0;
		virtual bool flush(int writer, int address, int data) = 0;
};

typedef	struct 
{
	bool valid;
	sc_uint<20> tag;
	int data[8]; //8 words = 32 byte line size 
} aca_cache_line;

typedef	struct 
{
	aca_cache_line cache_line[CACHE_LINES];
} aca_cache_set; 

typedef	struct
{
	aca_cache_set cache_set[CACHE_SETS];
} aca_cache;

class Cache;
class State
{
	virtual void processorRd(Cache *c)
	{
		//do nothing
	}
};




class Invalid : public State
{
	void processorRd(Cache *c, int addr);
	void isShared(Cache *c, int addr, int data);
	void notShared(Cache *c, int addr);
};

class Owned : public State
{

};
class Shared : public State
{
};
class Exclusive : public State
{

	void snoopedBusRd(Cache *c);

};



SC_MODULE(Cache) 
{

	public:
		enum BUS_REQ 
		{
			BUS_RD,
			BUS_WR,
			BUS_RDX,//-> seems have same response with BUS_WR
			BUS_INVALID,
			BUS_FLUSH
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

		// Interface port to issue commands to the Bus
		sc_port<Bus_if>	Port_Bus;

		int cache_id;	
		int snooping;


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

			current = new Invalid();
		}

		~Cache() 
		{
			delete cache;
			delete lru_table;

		}
		
		   void setCurrent(State *s)
		   {
		   current = s;

		   }
		 
	private:
		   class State *current;

		aca_cache *cache;
		unsigned char *lru_table;
		bool valid_lines[8];

		void processorRd(int addr)
		{
			current->processorRd(this, addr);
		}
		void isShared(int addr, int data)
		{
			current->isShared(this, addr, data);
		}
		void notShared(int addr)
		{
			current->notShared(this, addr);
		}

		void snoopedBusRd(int addr)
		{
			current->snoopedBusRd(this, addr);
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

		void snoop();

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
				bool hit   = false;
				int hit_set = -1;

				//determine whether a hit
				cout << "addr: " << hex << addr << endl;
				line_index = (addr & 0x00000FE0) >> 5;
				tag = addr >> 12;
				cout << "line_index: " << line_index <<  " tag: " <<tag << endl;
				word_index = ( addr & 0x0000001C ) >> 2;
				for ( int i=0; i <CACHE_SETS; i++ ){
					c_line = &(cache->cache_set[i].cache_line[line_index]);
					if (c_line -> valid == true){
						valid_lines[i] = true;	
						if ( c_line -> tag == tag){
							hit = true;
							hit_set=i; 
						}

					}
					else{
						valid_lines[i] = false;
					}
				}
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

				if (f == FUNC_WRITE) 
				{
					int cpu_data = Port_Data.read().to_int();

					cout << sc_time_stamp() << ": MEM received write" << endl;
					if (hit){ //write hit

						Port_Bus->write(cache_id, addr, cpu_data);//issue bus write for a write hit 
						stats_writehit(cache_id);

						Port_Hit.write(true);
						c_line = &(cache->cache_set[hit_set].cache_line[line_index]);

						c_line -> data[word_index] = cpu_data;
						wait();//consume 1 cycle
						cout << sc_time_stamp() << ": Cache write hit!" << endl;
						//sth need to do with lru
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
					else //write miss
					{		
						Port_Bus->readx(cache_id, addr, cpu_data);//issue bus readx when write miss -> didnt see rdx in this case
						stats_writemiss(cache_id);

						Port_Hit.write(false);
						cout << sc_time_stamp() << ": Cache write miss!" << endl;

						for ( int i=0; i <CACHE_SETS; i++ ){
							if (valid_lines[i] == false){ //use an invalid line
								// write allocate
								c_line = &(cache->cache_set[i].cache_line[line_index]);
								cout << "Write waiting for global access " << cache_id <<endl; 
								for (int j = 0; j < 8; j++)
								{
									wait(100); //fetch 8 * words data from memory to cache
									c_line -> data[j] = rand()%10000; //load the memory line(32bytes) to the appropriate cache line
								}
								c_line -> data[word_index] = cpu_data; //actual write from processor to cache line
								c_line -> valid = true;
								c_line -> tag = tag;
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
#if 1
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
								cout<< "Replacing now the cache line in set ....." << set_index_toreplace << endl;


								// write allocate
								for (int i = 0; i< 8; i++){
									wait(100); //fetch 8 * words data from memory to cache
									c_line -> data[i] = rand()%10000; //load the memory line(32bytes) to the appropriate cache line
								}

								// Replace the word in the cache line and make it valid
								c_line -> data[word_index] =  cpu_data; //actual write from processor to cache line
								c_line -> valid = true;
								c_line -> tag = tag;
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
#endif
						}
					}
#if 1
					//adding this becuase of write through
					for(int i=0; i<8;i++)//write the cache line back to the memory for both write his and miss
						wait(100);
#endif

					Port_Done.write( RET_WRITE_DONE );
				}
				else//a read comes to cache
				{
					cout << sc_time_stamp() << ": MEM received read" << endl;

					if (hit){ //read hit
						stats_readhit(cache_id);// do nothing for a read hit.

						Port_Hit.write(true);
						//Port_Hit_Line.write(hit_set);
						c_line = &(cache->cache_set[hit_set].cache_line[line_index]);

						Port_Data.write( c_line -> data[word_index] );
						cout << sc_time_stamp() << ": Cache read hit!" << endl;
						//some lru stuff needed to be done
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
					else //read miss
					{		
						Port_Bus->read(cache_id, addr); // issue a bus read for a read miss
						stats_readmiss(cache_id);

						Port_Hit.write(false);
						cout << sc_time_stamp() << ": Cache read miss!" << endl;

						for ( int i=0; i <CACHE_SETS; i++ ){
							if (valid_lines[i] == false){ //use an invalid line
								// write allocate
								c_line = &(cache->cache_set[i].cache_line[line_index]);

								for (int j = 0; j < 8; j++)
								{
									wait(100); //fetch 8 * words data from memory to cache
									c_line -> data[j] = rand()%10000; //load the memory line(32bytes) to the appropriate cache line
								}
								Port_Data.write(c_line -> data[word_index]); //return data to the CPU
								c_line -> valid = true;
								c_line -> tag = tag;
								//update the lru table after the cache update
								switch (i)
								{	case 0: lru_table[line_index] |=0b1101000; break;
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
#if 1
							else if(i == CACHE_SETS-1){// all lines are valid
								//lru
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
								cout<< "Replacing now the cache line in set ....." << set_index_toreplace << endl;

								//write back the previous line to mem 
								for(int i=0; i<8;i++)//write the cache line back to the memory
									wait(100);

								// write allocate
								for (int i = 0; i< 8; i++){
									wait(100); //fetch 8 * words data from memory to cache
									c_line -> data[i] = rand()%10000; //load the memory line(32bytes) to the appropriate cache line
								}

								// Replace the word in the cache line and make it valid
								Port_Data.write(c_line -> data[word_index]);//read from the cache line and give it to CPU 
								c_line -> valid = true;
								c_line -> tag = tag;
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
									default: cout << "Damn here !!!!" << endl;

								}
							}
#endif
						}
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

 
void Invalid :: processorRd(Cache *c, int addr) 
{
	//check if the addr is shared by others
	c->Port_Bus.read(c->cache_id, addr);
}

void Invalid :: isShared(Cache *c, int addr, int data) 
{
	//check if the addr is shared by others
	c->Port_Bus.read(c->cache_id, addr, data);
	c->setCurrent(new Shared());		
	delete this;
}

void Invalid :: notShared(Cache *c) 
{
	c->setCurrent(new Exclusive());		
	delete this;
}


void Exclusive :: snoopedBusRd(Cache *c)
{
	c->setCurrent(new Owned());
	delete this;
	flush();
}

void Cache::snoop()
{

	while (true)
	{
		// Snoop for requests on Bus
		wait(Port_BusReq.value_changed_event());
		int writer = Port_BusWriter.read().to_int();
		int data = Port_BusData.read().to_int();

		// If the writer on the bus is not myself, probe the bus
		if(writer != cache_id){
			// Fetch the address on the bus to check its local cache
			int addr= Port_BusAddr.read().to_int();
			aca_cache_line *c_line;
			sc_uint<20> tag = 0;
			unsigned int line_index;
			line_index = (addr & 0x00000FE0) >> 5;
			tag = addr >> 12;
			int req = Port_BusReq.read().to_int();

			switch(req)
			{
				case BUS_RD:
					/* do nothing
					 */
					ProbeReads++;
				        this.snoopedBusRd();	
					break;

				case BUS_RDX:

				case BUS_WR:
					/* Invalidate the cache line with 
					 * the corresponding address
					 */
					/*
					for ( int i=0; i <CACHE_SETS; i++ ){
						c_line = &(cache->cache_set[i].cache_line[line_index]);
						if (c_line -> valid == true){
							if ( c_line -> tag == tag){
								c_line -> valid = false;
							}

						}
					}
					*/
					ProbeWrites++;

					break;

				case BUS_FLUSH:
					for ( int i=0; i <CACHE_SETS; i++ ){
						c_line = &(cache->cache_set[i].cache_line[line_index]);
						if (c_line -> valid == true){
							if ( c_line -> tag == tag){
								
							}

						}
					}
					/*snooper detect that somebody flushes the thing I need */
					this.isShared(addr,data);
					break;

				default://BUS_INVALID command
					cout<<"a invalid command was issued in the end"<<endl;
					break;
			}
		}
		wait();
	}

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
			Port_BusReq.write(0x00);

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
			Port_BusReq.write(0x01);

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
			Port_BusReq.write(0x02);
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

		virtual bool flush(int writer, int addr, int data) 
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
