#include <tuple>    // tuple, tie
#include <cstddef>  // Null
#include <bits/stdc++.h> //log
#include <stdio.h> // fopen, fscanf
#include <stdlib.h> // atoi
#include <inttypes.h> // uint32_t


#define addressSize 32
unsigned int totalMemoryAccess = 0;

// class, struct definitions and  constructors
typedef struct {
   uint32_t BLOCKSIZE;
   uint32_t L1_SIZE;
   uint32_t L1_ASSOC;
   uint32_t L2_SIZE;
   uint32_t L2_ASSOC;
   uint32_t PREF_N;
   uint32_t PREF_M;
} cache_params_t;

struct StreamBuffer
{
    bool valid;
    int* buffer;
    int lru;
    int head;

};

class Block
{
        
    public:
        bool valid;
        bool dirty;
        unsigned int tag;
        int lru;

        Block()
        {
            this->valid = false;
            this->dirty = false;
            this->tag = 0x0;
            this->lru = -1;
        }

        void display_block_information()
        {
            std::cout << "Valid : "<<this->valid << std::endl;
            std::cout << "Dirty : "<<this->dirty << std::endl;
            std::cout << "lru : "<<this->lru << std::endl;
        }
};

class Cache
{
    // cache geometry
    int cacheSize;
    int assoc;
    int blockSize;
    int setCount;

    // Set Organization within Cache 
    Block** sets;


    // Next Level Cache Pointer
    Cache* nextLevel;

    //tag, index and blockoffset bits
    int tagBits;
    int indexBits;
    int blockOffsetBits;
    
    public:

    // cache parameters
    int reads;
    int readMisses;
    int writes;
    int writeMisses;
    int writebacks;
    

    // helper methods
    void get_split_bits(int cacheSize, int assoc, int blockSize);
    std::tuple <unsigned int, unsigned int, unsigned int> get_split_address(unsigned int address);
    unsigned int concatenate_victim_address(unsigned int tag, unsigned int index, unsigned int tagBits, unsigned int indexBits, unsigned int blockOffsetBits);
    
    // constructor
    Cache(int cacheSize, int assoc, int blockSize, Cache* nextLevel, int prefN, int prefM);

    // display Cache Cache Characteristics Methods
    void display_cache_gemoetry();
    void display_cache_access_params();
    void display_cache_address_split_bits();
    void display_contents();
    void display_sets();

    // read request for cache 
    void read(unsigned int address);  
    void write(unsigned int address); 
    
    //stream buffer
    StreamBuffer* sb;
    int prefetches;
    int prefN;
    int prefM;
    void display_stream_buffer();
    void flush_buffer(StreamBuffer* sb, int blockXAddress);
    void update_buffer(StreamBuffer* sb, int blockXAddress, int blockXAddressPosition);
    int check_buffer(StreamBuffer* sb, int blockXAddress);
    void prefetch_read();
    std::tuple<int, int> check_buffer_stream(int blockXAddress);
    
};

Cache :: Cache(int cacheSize, int assoc, int blockSize, Cache* nextLevel, int prefN, int prefM)
{
        // initialize cache geometry parameters
        this->cacheSize = cacheSize;
        this->assoc = assoc;
        this->blockSize = blockSize;

        // initialize cache access parameters
        this->reads = 0;
        this->readMisses = 0;
        this->writes = 0;
        this->writeMisses = 0;
        this->writebacks = 0;

        // initialize the address, split bits and #sets
        get_split_bits(cacheSize, assoc, blockSize);

        // define the cache geometry
        /*
        sets is an array of pointer to pointers.
        The individual pointers in sets point to an array of blocks.
        */

        this->sets = new Block*[this->setCount]; 
        for(int i=0; i<this->setCount ; i++){
            sets[i] = new Block[assoc];
            for(int j =0; j<assoc; j++){
                sets[i][j].lru =j;
            }
        }
        
        // define the next level in the memory hierarchy
        this->nextLevel = nextLevel;

        // define the stream buffer
        this->prefN = prefN;
        this->prefM = prefM;
        if(this->prefN != 0){
            this->sb = new StreamBuffer[this->prefN];
            for(int i=0; i<this->prefN; i++){
                sb[i].buffer = new int[prefM];
                sb[i].lru = i;
            }
        }
        else
            this->sb = NULL;
        this->prefetches = 0;
        
}


// addresss splitting concatenating methods
void Cache :: get_split_bits(int cacheSize, int assoc, int blockSize)
{
    //int indexBits, blockaOffsetBits, tagBits;
    this->setCount = cacheSize / (assoc * blockSize);
    
    this->blockOffsetBits = std::log(blockSize)/std::log(2);
    this->indexBits = std::log(this->setCount)/std::log(2);
    this->tagBits = addressSize - this->indexBits - this->blockOffsetBits;
    
}

std::tuple <unsigned int, unsigned int, unsigned int> Cache :: get_split_address(unsigned int address)
{
    unsigned int blockOffsetMask = ((0x1 << this->blockOffsetBits) - 1);
    unsigned int blockOffset = address & blockOffsetMask;
    address = address >> this->blockOffsetBits;

    unsigned int indexMask = ((0x1 << indexBits) - 1);
    unsigned int index = address & indexMask;
    
    address = address >> this->indexBits;
    unsigned int tag = address;

    return std::make_tuple(tag, index, blockOffset);
    
}
unsigned int Cache :: concatenate_victim_address(unsigned int tag, unsigned int index, unsigned int tagBits, unsigned int indexBits, unsigned int blockOffsetBits)
{
   unsigned int address;
   address = 0x0;
   address = (address | tag) << (32 - tagBits);
   address = address | (index << blockOffsetBits);
   return address; 
}

// display methods 
void display_measurements(Cache* L1, Cache* L2)
{
    std::cout << "===== Measurements =====" << std::endl;
    std::cout << "a. L1 reads:                   " << L1->reads << std::endl;
    std::cout << "b. L1 read misses:             " << L1->readMisses << std::endl;
    std::cout << "c. L1 writes:                  " << L1->writes << std::endl;
    std::cout << "d. L1 write misses:            " << L1->writeMisses << std::endl;
    printf("e. L1 miss rate:               %.4f\n",(float)(L1->writeMisses + L1->readMisses)/(L1->reads + L1->writes));
    std::cout << "f. L1 writebacks:              " << L1->writebacks << std::endl;
    if(L1->sb == NULL)
        std::cout << "g. L1 prefetches:              0" << std::endl;
    else
        std::cout << "g. L1 prefetches:              " << L1->prefetches << std::endl;


    if(L2==NULL){
       
        std::cout << "h. L2 reads (demand):          0" << std::endl;
        std::cout << "i. L2 read misses (demand):    0" << std::endl;
        std::cout << "j. L2 reads (prefetch):        0" << std::endl;
        std::cout << "k. L2 read misses (prefetch):  0" << std::endl;
        std::cout << "l. L2 writes:                  0" << std::endl;
        std::cout << "m. L2 write misses:            0" << std::endl;
        std::cout << "n. L2 miss rate:               0.0000" << std::endl;
        std::cout << "o. L2 writebacks:              0" << std::endl;
        std::cout << "p. L2 prefetches:              0" << std::endl;
        
    }
    else{
       
        std::cout << "h. L2 reads (demand):          " << L2->reads << std::endl;
        std::cout << "i. L2 read misses (demand):    " << L2->readMisses <<std::endl;
        std::cout << "j. L2 reads (prefetch):        0" << std::endl;
        std::cout << "k. L2 read misses (prefetch):  0" << std::endl;
        std::cout << "l. L2 writes:                  " << L2->writes << std::endl;
        std::cout << "m. L2 write misses:            " << L2->writeMisses << std::endl;
        printf("n. L2 miss rate:               %.4f\n", (float)(L2->readMisses)/(L2->reads));
        std::cout << "o. L2 writebacks:              " << L2->writebacks << std::endl ;
         if(L2->sb == NULL)
            std::cout << "p. L2 prefetches:              0" << std::endl;
        else
            std::cout << "p. L2 prefetches:              " << L2->prefetches << std::endl;
    
    }
    std::cout << "q. memory traffic:             " << totalMemoryAccess << std::endl;
}

void Cache :: display_contents()
{   
    Block* set;
    set = new Block[this->assoc];
    for(int i=0; i<this->setCount; i++){

        for(int k=0; k<this->assoc; k++)
            set[sets[i][k].lru] = sets[i][k];

        std::cout << "set      " << i << ":"; 
        for(int j=0; j<this->assoc; j++){
            if(set[j].dirty)
            	printf("   %0x D ",set[j].tag);
	    else
      		printf("   %0x  ",set[j].tag);
                           
        }
        std::cout << std::endl;
       
    }
}

void display_simulator(cache_params_t params, Cache* L1, Cache* L2, char* trace_file)
{
    // Print simulator configuration.
   printf("===== Simulator configuration =====\n");
   printf("BLOCKSIZE:  %u\n", params.BLOCKSIZE);
   printf("L1_SIZE:    %u\n", params.L1_SIZE);
   printf("L1_ASSOC:   %u\n", params.L1_ASSOC);
   printf("L2_SIZE:    %u\n", params.L2_SIZE);
   printf("L2_ASSOC:   %u\n", params.L2_ASSOC);
   printf("PREF_N:     %u\n", params.PREF_N);
   printf("PREF_M:     %u\n", params.PREF_M);
   printf("trace_file: %s\n", trace_file);
   printf("\n");
   std::cout << "===== L1 contents =====\n";
   L1->display_contents();
   std::cout << "\n";

   if(L2!=NULL){
    std::cout << "===== L2 contents =====\n";
    L2->display_contents();
    printf("\n");
   }

   if(L1->sb != NULL)
    L1->display_stream_buffer();  

   if(L2 != NULL && L2->sb != NULL)
    L2->display_stream_buffer();   
   
   display_measurements(L1, L2);
   
    
}

void Cache :: display_stream_buffer()
{
    int lruCounter = 0;
    std::cout << "===== Stream Buffer(s) contents =====";
    int i=0;
    int lru;
    while(lruCounter <prefN && i < prefN*prefN){
        lru = i%prefN;
        if(this->sb[lru].lru == lruCounter){
                 std::cout << "\n ";
                for(int j=this->sb[lru].head; j<this->prefM; j++)
                    printf(" %0x", this->sb[lru].buffer[j] );
                
                for(int k=0; k<this->sb[lru].head; k++)
                    printf(" %0x", this->sb[lru].buffer[k] );
                
                lruCounter++;
            }
        i++;
    }
        std::cout << "\n\n"; 
}


// stream buffer access methods

int Cache :: check_buffer(StreamBuffer* sb, int blockXAddress)
{
    for(int i=sb->head; i<this->prefM; i++){
        if(sb->buffer[i] == blockXAddress)
            return i;
    }
    for(int i=0; i<sb->head; i++){
        if(sb->buffer[i] == blockXAddress)
            return i;
    }
    return -1;
}

std::tuple<int, int> Cache :: check_buffer_stream(int blockXAddress)
{   
        int lruCounter = 0; 
        int blockXPosition = -1;
        int bufferPosition = -1;
        int i =0;
        if(this->prefN > 0){

            while(i<this->prefN*this->prefN && lruCounter < this->prefN)
            {
                if(this->sb[i%this->prefN].lru == lruCounter && this->sb[i%this->prefN].valid)
                {
                    blockXPosition = this->check_buffer(&this->sb[i%this->prefN], blockXAddress);
                    lruCounter++;
                    if(blockXPosition!=-1){
                    bufferPosition = i%this->prefN;
                    break;
                    }
                    
                }
                i++;
            }
        }
        return std::make_tuple(bufferPosition, blockXPosition);
}

void Cache :: prefetch_read()
{
    /*stream buffer only on the last level in the memory hierarchy always so no prefetch read requests from
    L1 to L2.*/
    totalMemoryAccess++;
    this->prefetches++;

}

void Cache :: flush_buffer(StreamBuffer* sb, int blockXAddress)
{
    // cache miss + sb miss
    for(int i=0; i<this->prefM; i++){
        this->prefetch_read();
        sb->buffer[i] = blockXAddress+ i + 1;
    }
    sb->head = 0;
    sb->valid = 1;
}

void Cache :: update_buffer(StreamBuffer* sb, int blockXAddress, int blockXAddressPosition)
{
    // cache miss + sb hit
    
    
    // update the data in stream buffer 
    // cache miss + sb hit
    int nextBlockAddress = blockXAddress + 1;

    // update head
    sb->head = (blockXAddressPosition == this->prefM-1) ? 0 : blockXAddressPosition + 1;
    
    // update the data in stream buffer 
    if(sb->head == 0){
        for(int i=0 ;i<this->prefM;i++){
            if(sb->buffer[i] != nextBlockAddress)
                this->prefetch_read();

            sb->buffer[i] = nextBlockAddress;
            nextBlockAddress++;
        }
    }
    else{
        for(int i=sb->head ; i<this->prefM; i++){
            if(sb->buffer[i] != nextBlockAddress)
		        this->prefetch_read();
            sb->buffer[i] = nextBlockAddress;
            nextBlockAddress++;
        }
        for(int i=0; i<sb->head;i++){
	   if(sb->buffer[i] != nextBlockAddress)
             	this->prefetch_read();
            sb->buffer[i] = nextBlockAddress;
            nextBlockAddress++;
        }
    }
    sb->valid = 1;
    
    
}

// read
void Cache :: read(unsigned int address)
{
    // split address into tag, index and blockoffset
    unsigned int tag, index, blockOffset;
    std::tie(tag, index, blockOffset)= this->get_split_address(address);
    
    // generate the block address for the address
    int blockAddress = address >> this->blockOffsetBits;

    // check the cache for a read hit 
    int readHitFlag = 0;
    int targetBlock = -1;
    for(int i=0 ;i < this->assoc; i++){
        if(sets[index][i].tag == tag && sets[index][i].valid == true){            
            readHitFlag = 1;
            targetBlock = i;
            break;
        }
    }

    int bufferPosition, blockPosition;
    // check the stream buffer for a buffer hit
    std::tie(bufferPosition, blockPosition) = this->check_buffer_stream(blockAddress);
    int kuthe = 0;

    if(this->prefN > 0){// cache and sb simulation
        
        if(bufferPosition == -1 && readHitFlag == 0)
        {   kuthe = 1;
            // buffer miss cache miss

            // buffer
                // find lru buffer
                int targetBuffer = -1;
                for(int i=0; i<this->prefN; i++){
                    if(this->sb[i].lru == prefN - 1){
                    targetBuffer = i;
                    break;
                    }
                }

                // flush lru buffer
                this->flush_buffer(&this->sb[targetBuffer], blockAddress);
                
                // update target buffer lru
                for(int i =0; i< this->prefN; i++){
                    if(this->sb[i].lru < this->sb[targetBuffer].lru)
                        this->sb[i].lru++;
                }
                this->sb[targetBuffer].lru = 0;
            
            // cache
                // update read misses
                this->readMisses++;

                // find the block with lru = assoc - 1 
                 for(int i=0; i<assoc; i++){
                    if(sets[index][i].lru == assoc-1){
                        targetBlock = i;
                        break;
                    }
                }
                // next level logic only for next level as memory as simulator supports stream buffer only at lowest level
                if(this->sets[index][targetBlock].dirty){
                    this->writebacks++;
                    totalMemoryAccess++;
                 }
                totalMemoryAccess++;
                
        }
        else if(bufferPosition == -1 && readHitFlag == 1)
        {kuthe = 2;
            // buffer miss cache hit

            // buffer
                // nothing 
            // cache
                // nothing 
        }
        else if(bufferPosition!=-1 && readHitFlag == 0)
        {kuthe = 3;
            // buffer hit cache miss

            // buffer 
                // update buffer  
                this->update_buffer(&this->sb[bufferPosition], blockAddress, blockPosition);
                
                // update buffer lru
                for(int i =0; i< this->prefN; i++){
                    if(this->sb[i].lru < this->sb[bufferPosition].lru)
                        this->sb[i].lru++;
                }
                this->sb[bufferPosition].lru = 0;

                // find the block with lru = assoc - 1 
                for(int i=0; i<assoc; i++){
                if(sets[index][i].lru == assoc-1){
                    targetBlock = i;
                    break;
                    }
                } 
            
            // cache
                // next level logic only for next level as memory and dirty eviction as simulator supports stream buffer only at lowest level.
                if(this->sets[index][targetBlock].dirty){
                this->writebacks++;
                totalMemoryAccess++;
                }


        }
        else if(bufferPosition!=-1 && readHitFlag == 1)
        {kuthe = 4;
            // buffer hit cache hit

            // buffer
                // update buffer  
                this->update_buffer(&this->sb[bufferPosition], blockAddress, blockPosition);
                
                // update buffer lru
                for(int i =0; i< this->prefN; i++){
                    if(this->sb[i].lru < this->sb[bufferPosition].lru)
                        this->sb[i].lru++;
                }
                this->sb[bufferPosition].lru = 0;

            // cache
                // nothing
        }
        else
        {
            // forbidden else :  under right condition this else will never be executed.
            // present for bug fixes and corner case detection.
        }

    }
    else{// only cache simulation
            if(!readHitFlag){
                kuthe = 5;

                // find the block with lru = assoc - 1 
                for(int i=0; i<assoc; i++){
                    if(sets[index][i].lru == assoc-1){
                        targetBlock = i;
                        break;
                    }
                }

                // update read misses
                this->readMisses++;

                // next level is memory then update memory accesses
                if(this->nextLevel == NULL){
                    if(this->sets[index][targetBlock].dirty){
                        this->writebacks++;
                        totalMemoryAccess++;
                    }
                    totalMemoryAccess++;
                        
                }
                // next level a cache 
                else{
                    // dirty victim block eviction 
                    // 1. concatenate victim address 
                    // 2. write request for the dirty victim
                    if(this->sets[index][targetBlock].dirty){
                        int victimAddress = this->concatenate_victim_address(sets[index][targetBlock].tag, index, this->tagBits, this->indexBits, this->blockOffsetBits);
                        this->nextLevel->write(victimAddress);
                        this->writebacks++;
                    }
               
                    // issue read request of the current block to the next level.
                    this->nextLevel->read(address);
                }
            }
    }

    // update lru information
    for(int i =0; i< assoc; i++){
            if(sets[index][i].lru < sets[index][targetBlock].lru)
                sets[index][i].lru++;
    }

    // update block information
    sets[index][targetBlock].lru = 0;
    sets[index][targetBlock].tag = tag;
    sets[index][targetBlock].valid = true;
    sets[index][targetBlock].dirty = (readHitFlag)?sets[index][targetBlock].dirty:false; // update dirty differently for hit and miss.
    // update reads
     this->reads++;
    // printf("\n read address : %0x  index : %0x cacheHit %d kuthe : %d bufferPosition : %d  targetBlock : %d blockXPosition : %d", address, index, readHitFlag, kuthe, bufferPosition, targetBlock, blockPosition);
    // int bufferHead = (bufferPosition == -1) ? -1 : this->sb[bufferPosition].head;
    // //printf(" bufferHead : %d", bufferHead);
    // std::cout << "\n stream buffer after read";
    // this->display_stream_buffer();
    // std::cout << "\n cache after read";
    // this->display_contents();
}
     



// write 
void Cache :: write(unsigned int address)
{
    
    // split address into tag, index and blockoffset
    unsigned int tag, index, blockOffset;
    std::tie(tag, index, blockOffset)= this->get_split_address(address);
    
    // generate the block address for the address
    int blockAddress = address >> this->blockOffsetBits;
    
    // check the cache for a write hit 
    int writeHitFlag = 0;
    int targetBlock = -1;
    for(int i=0 ;i < this->assoc; i++){
        if(sets[index][i].tag == tag && sets[index][i].valid == true){            
            writeHitFlag = 1;
            targetBlock = i;
            break;
        }
    }

    int bufferPosition, blockPosition;
    // check the stream buffer for a buffer hit
    std::tie(bufferPosition, blockPosition) = this->check_buffer_stream(blockAddress);
    int kuthe = 0;

    if(this->prefN > 0){ // cache and buffer sim
        if(bufferPosition == -1 && writeHitFlag == 0)
        {
            // buffer miss cache miss
            kuthe = 1;
            // buffer
                // find lru buffer
                int targetBuffer = -1;
                for(int i=0; i<this->prefN; i++){
                    if(this->sb[i].lru == prefN - 1){
                    targetBuffer = i;
                    break;
                    }
                }

                // flush lru buffer
                this->flush_buffer(&this->sb[targetBuffer], blockAddress);
                

                // update target buffer lru
                for(int i =0; i< this->prefN; i++){
                    if(this->sb[i].lru < this->sb[targetBuffer].lru)
                        this->sb[i].lru++;
                }
                this->sb[targetBuffer].lru = 0;
            
            // cache
                //update write misses
                this->writeMisses++;

                // find the block with lru = assoc - 1 
                 for(int i=0; i<assoc; i++){
                    if(sets[index][i].lru == assoc-1){
                        targetBlock = i;
                        break;
                    }
                }
                // next level logic only for next level as memory as simulator supports stream buffer only at lowest level
                if(this->sets[index][targetBlock].dirty){
                    this->writebacks++;
                    totalMemoryAccess++;
                 }
                totalMemoryAccess++;
                
        }
        else if(bufferPosition == -1 && writeHitFlag == 1)
        {   kuthe = 2;
            // buffer miss cache hit

            // buffer
                // nothing 
            // cache
                // nothing 
        }
        else if(bufferPosition!=-1 && writeHitFlag == 0)
        {kuthe = 3;
            // buffer hit cache miss

            // buffer 
                // update buffer  
                this->update_buffer(&this->sb[bufferPosition], blockAddress, blockPosition);
                
                // update buffer lru
                for(int i =0; i< this->prefN; i++){
                    if(this->sb[i].lru < this->sb[bufferPosition].lru)
                        this->sb[i].lru++;
                }
                this->sb[bufferPosition].lru = 0;

                // find the block with lru = assoc - 1 
                for(int i=0; i<assoc; i++){
                if(sets[index][i].lru == assoc-1){
                    targetBlock = i;
                    break;
                    }
                } 
            
            // cache
                // next level logic only for next level as memory and dirty eviction as simulator supports stream buffer only at lowest level and its a buffer hit.
                if(this->sets[index][targetBlock].dirty){
                this->writebacks++;
                totalMemoryAccess++;
                }
        }
        else if(bufferPosition!=-1 && writeHitFlag == 1)
        {kuthe = 4;
            // buffer hit cache hit

            // buffer
                // update buffer 
                this->update_buffer(&this->sb[bufferPosition], blockAddress, blockPosition);
                
                // update buffer lru
                for(int i =0; i< this->prefN; i++){
                    if(this->sb[i].lru < this->sb[bufferPosition].lru)
                        this->sb[i].lru++;
                }
                this->sb[bufferPosition].lru = 0;

            // cache
                // nothing
        }
        else
        {
            // forbidden else :  under right condition this else will never be executed.
            // present for bug fixes and corner case detection.
        }

    }
    else{ // only cache sim
        if(!writeHitFlag){
            kuthe = 5;
            // update write misses
             this->writeMisses++;

            // find the block with lru = assoc - 1 
            for(int i=0; i<assoc; i++){
                if(sets[index][i].lru == assoc-1){
                    targetBlock = i;
                    break;
                    }
            } 

            if(this->nextLevel == NULL){
                if(this->sets[index][targetBlock].dirty){
                    this->writebacks++;
                    totalMemoryAccess++;
                }
                totalMemoryAccess++;
            }
            else{
                // dirty victim block eviction 
                if(sets[index][targetBlock].dirty){
                    int victimAddress = this->concatenate_victim_address(sets[index][targetBlock].tag, index, this->tagBits, this->indexBits, this->blockOffsetBits);
                    this->nextLevel->write(victimAddress);
                    this->writebacks++;
                    }
               
                // issue write request of the current block to the next level.
                this->nextLevel->read(address);
            }
        }
    }
    
     // update lru information
    for(int i =0; i< assoc; i++){
            if(sets[index][i].lru < sets[index][targetBlock].lru)
                sets[index][i].lru++;
        }
    // update block information
    sets[index][targetBlock].lru = 0;
    sets[index][targetBlock].tag = tag;
    sets[index][targetBlock].valid = true;
    sets[index][targetBlock].dirty = true;
    // update writes
    this->writes++;
    //printf("\n Write address : %0x  index : %0x cacheHit %d kuthe : %d bufferPosition : %d  targetBlock : %d blockXPosition : %d", address, index, writeHitFlag, kuthe, bufferPosition, targetBlock, blockPosition);
    //int bufferHead = (bufferPosition == -1) ? -1 : this->sb[bufferPosition].head;
    // printf(" bufferHead : %d", bufferHead);

    // std::cout << "\n stream buffer after write";
    // this->display_stream_buffer();
    // std::cout << "\n cache after write";
    // this->display_contents();
}


int main (int argc, char *argv[]) {
   FILE *fp;			
   char *trace_file;		
   cache_params_t params;	
   char rw;			
   uint32_t addr;		
			
   // Exit with an error if the number of command-line arguments is incorrect.
   if (argc != 9) {
      printf("Error: Expected 8 command-line arguments but was provided %d.\n", (argc - 1));
      exit(EXIT_FAILURE);
   }
    
   params.BLOCKSIZE = (uint32_t) atoi(argv[1]);
   params.L1_SIZE   = (uint32_t) atoi(argv[2]);
   params.L1_ASSOC  = (uint32_t) atoi(argv[3]);
   params.L2_SIZE   = (uint32_t) atoi(argv[4]);
   params.L2_ASSOC  = (uint32_t) atoi(argv[5]);
   params.PREF_N    = (uint32_t) atoi(argv[6]);
   params.PREF_M    = (uint32_t) atoi(argv[7]);
   trace_file       = argv[8];
      
      
    Cache* L2;
    L2 = (params.L2_SIZE!=0) ? new Cache(params.L2_SIZE, params.L2_ASSOC, params.BLOCKSIZE, NULL, params.PREF_N, params.PREF_M) : NULL;

    Cache* L1 = (L2 == NULL ) ? new Cache(params.L1_SIZE,params.L1_ASSOC,params.BLOCKSIZE,L2, params.PREF_N, params.PREF_M) :new Cache(params.L1_SIZE,params.L1_ASSOC,params.BLOCKSIZE,L2, 0, 0);

   // Open the trace file for reading.
   fp = fopen(trace_file, "r");
   if (fp == (FILE *) NULL) {
      // Exit with an error if file open failed.
      printf("Error: Unable to open file %s\n", trace_file);
      exit(EXIT_FAILURE);
   }
    
   
   // Read requests from the trace file and echo them back.
   while (fscanf(fp, "%c %x\n", &rw, &addr) == 2) {	// Stay in the loop if fscanf() successfully parsed two tokens as specified.
      if (rw == 'r')
         L1->read(addr);
        
      else if (rw == 'w')
         L1->write(addr);
        
      else {
         printf("Error: Unknown request type %c.\n", rw);
	     exit(EXIT_FAILURE);
        printf("\n %c %x", rw, addr);
      }
    }

    display_simulator(params, L1, L2, trace_file);

    return 0;
}

