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

#include "aca2009.h"
#include <systemc.h>
#include <iostream>
#include <iomanip> 

using namespace std;

//static const int MEM_SIZE = 512;

#define CACHE_SETS 8
#define CACHE_LINES 128


typedef	struct 
{
	bool valid;
	sc_uint<20> tag;
	//sc_int<8> data[32]; //32 byte line size 
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

SC_MODULE(Cache) 
{

	public:

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
		sc_out<bool>     Port_Wr_Done;
		sc_out<bool>     Port_Wr_Func;
		sc_out<int> 	Port_Hit_Line;
		sc_out<int> 	Port_Replace_Line;

		SC_CTOR(Cache) 
		{
			SC_THREAD(execute);
			sensitive << Port_CLK.pos();
			dont_initialize();

			//m_data = new int[MEM_SIZE];
			cache = new aca_cache;
			lru_table= new unsigned char[CACHE_LINES] ;
			for (int i = 0; i<8; i++)
				valid_lines[i] = false;
			for (int j = 0; j< CACHE_LINES; j++)
				lru_table[j] = 0;
		}

		~Cache() 
		{
			//delete[] m_data;
			delete cache;
			delete lru_table;

		}
	private:
		aca_cache *cache;
		unsigned char *lru_table;
		bool valid_lines[8];
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


		aca_cache_line* updateLRU(int addr);

		void execute() 
		{
			while (true)
			{
				//cout<<"waiting for wr function"<<endl;
				wait(Port_Func.value_changed_event());
				//cout<<"get wr function"<<endl;

				Function f = Port_Func.read();
				int addr   = Port_Addr.read();
				Port_Wr_Func.write(f);
				//int *data;
				aca_cache_line *c_line;
				sc_uint<20> tag = 0;
				unsigned int line_index;
				unsigned int word_index = 0;
				bool hit   = false;
				int hit_set = -1;
				//cout << sc_time_stamp() << " Function is " << f << endl;

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

				cout << "before replacing--------------" <<endl;
				cout<<"lru_table: "<<binary(lru_table[line_index])<<endl;
				cout <<setw(8) << "set"<< setw(8) <<  "valid" << setw(8) <<  "tag" <<endl;
				for (int set = 0; set < 8; set++){ 
					c_line = &(cache->cache_set[set].cache_line[line_index]);
					cout <<setw(8)<<  set <<setw(8) << c_line -> valid << setw(8)<< c_line -> tag <<endl; 

				}

				c_line = updateLRU(addr);

				if (f == FUNC_WRITE) 
				{
					int cpu_data = Port_Data.read().to_int();

					cout << sc_time_stamp() << ": MEM received write" << endl;
					if (hit){ //write hit
						Port_Hit.write(true);
						Port_Hit_Line.write(hit_set);
						stats_writehit(0);

						c_line -> data[word_index] = cpu_data;
						wait();//consume 1 cycle
					}
					else //write miss
					{		
						Port_Hit.write(false);
						stats_writemiss(0);
						cout << sc_time_stamp() << ": Cache write miss!" << endl;

						c_line -> valid = true;
						c_line -> tag = tag;

						// write allocate
						for (int i = 0; i< 8; i++){//here I think can be interrupted by someone else's flush
							wait(100); //fetch 8 * words data from memory to cache
							c_line -> data[i] = rand()%10000; //load the memory line(32bytes) to the appropriate cache line
						}
						c_line -> data[word_index] =  cpu_data; //actual write from processor to cache line

					}
					Port_Done.write( RET_WRITE_DONE );
					Port_Wr_Done.write ( RET_WRITE_DONE );

					//cout << sc_time_stamp() << ": MEM received write" << endl;
					//data = Port_Data.read().to_int();
				}
				else//a read comes to cache
				{
					cout << sc_time_stamp() << ": MEM received read" << endl;

					if (hit){ //read hit
						Port_Hit.write(true);
						Port_Hit_Line.write(hit_set);
						stats_readhit(0);


						Port_Data.write( c_line -> data[word_index] );

					}
					else //read miss
					{		
						Port_Hit.write(false);
						stats_readmiss(0);
						cout << sc_time_stamp() << ": Cache read miss!" << endl;

						c_line -> valid = true;
						c_line -> tag = tag;

						// write allocate
						for (int i = 0; i< 8; i++){//here I think can be interrupted by someone else's flush
							wait(100); //fetch 8 * words data from memory to cache
							c_line -> data[i] = rand()%10000; //load the memory line(32bytes) to the appropriate cache line
						}
						Port_Data.write(c_line -> data[word_index]); //return data to the CPU


					}


					//wait();
					Port_Wr_Done.write ( RET_READ_DONE );
					Port_Done.write( RET_READ_DONE );
					wait();
					Port_Data.write("ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ");
				}
				//at here means a read or a write has happened
				cout << "after replacing----------------------" <<endl;
				cout <<"lru_table: "<<binary(lru_table[line_index])<<endl;
				//cout <<"use invalid line in set: "<<
				cout <<setw(8) << "set"<< setw(8) <<  "valid" << setw(8) <<  "tag" <<endl;
				for (int set = 0; set < 8; set++){
					c_line = &(cache->cache_set[set].cache_line[line_index]);
					cout <<setw(8)<<  set <<setw(8) << c_line -> valid << setw(8)<< c_line -> tag <<endl; 
				}
				cout<<endl;

			}
		}
}; 




aca_cache_line* Cache::updateLRU(int addr)
{

	aca_cache_line *c_line;
	sc_uint<20> tag = 0;
	unsigned int line_index;
	line_index = (addr & 0x00000FE0) >> 5;
	tag = addr >> 12;
	bool hit   = false;
	int hit_set = -1;

	line_index = (addr & 0x00000FE0) >> 5;
	tag = addr >> 12;

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
	if(hit){
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

				for(int i=0; i<8;i++)//write the cache line back to the memory
					wait(100);

				/*
				// write allocate
				for (int i = 0; i< 8; i++){ 
				wait(100); //fetch 8 * words data from memory to cache
				c_line -> data[i] = rand()%10000; //load the memory line(32bytes) to the appropriate cache line
				}
				 */
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




SC_MODULE(CPU) 
{

	public:
		sc_in<bool>                Port_CLK;
		sc_in<Cache::RetCode>   Port_MemDone;
		sc_out<Cache::Function> Port_MemFunc;
		sc_out<int>                Port_MemAddr;
		sc_inout_rv<32>            Port_MemData;

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
				if(!tracefile_ptr->next(0, tr_data))
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
						cout << sc_time_stamp() << ": CPU sends write" << endl;

						uint32_t data = rand();
						Port_MemData.write(data);
						wait(); //this waiting for 1 cycle is mapping to the one cycle wait in the cache write hit.
						Port_MemData.write("ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ");
					}
					else
					{
						cout << sc_time_stamp() << ": CPU sends read" << endl;
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

		// Instantiate Modules
		Cache mem("main_memory");
		CPU    cpu("cpu");

		// Signals
		sc_buffer<Cache::Function> sigMemFunc;
		sc_buffer<Cache::RetCode>  sigMemDone;
		sc_signal<int>              sigMemAddr;
		sc_signal_rv<32>            sigMemData;
		sc_signal<bool> 	sigMemHit;
		sc_signal<bool>              sigMemWr_Done;
		sc_signal<bool>              sigMemWr_Func;
		sc_signal<int>              sigMemHitLine;
		sc_signal<int>              sigMemReplaceLine;
		// The clock that will drive the CPU and Cache
		sc_clock clk;

		// Connecting module ports with signals
		mem.Port_Func(sigMemFunc);
		mem.Port_Addr(sigMemAddr);
		mem.Port_Data(sigMemData);
		mem.Port_Done(sigMemDone);
		mem.Port_Hit(sigMemHit);
		mem.Port_Wr_Done(sigMemWr_Done);
		mem.Port_Wr_Func(sigMemWr_Func);
		mem.Port_Hit_Line(sigMemHitLine);
		mem.Port_Replace_Line(sigMemReplaceLine);

		cpu.Port_MemFunc(sigMemFunc);
		cpu.Port_MemAddr(sigMemAddr);
		cpu.Port_MemData(sigMemData);
		cpu.Port_MemDone(sigMemDone);

		mem.Port_CLK(clk);
		cpu.Port_CLK(clk);

		cout << "Running (press CTRL+C to interrupt)... " << endl;

		sc_trace_file *wf = sc_create_vcd_trace_file("CPU_MEM");
		// Dump the desired signals
		sc_trace(wf, clk, "clock");
		//sc_trace(wf, sigMemFunc, "wr");//does not showup
		//sc_trace(wf, sigMemDone, "ret");//does not showup
		sc_trace(wf, sigMemAddr, "addr");
		sc_trace(wf, sigMemData, "data");
		//sc_trace(wf, sigMemWr_Done, "wr_done");
		sc_trace(wf, sigMemWr_Func, "wr_func"); //function issued by cpu
		sc_trace(wf, sigMemHit, "Hit");
		sc_trace(wf, sigMemHitLine, "Hit_line");// show which set of cache line is hit
		sc_trace(wf, sigMemReplaceLine, "replace_line"); //show which set of cache line is replaced


		// Start Simulation
		//sc_start(2000000,SC_NS);
		sc_start();


		// Print statistics after simulation finished
		stats_print();
	}

	catch (exception& e)
	{
		cerr << e.what() << endl;
	}

	return 0;
}
