#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "icm/icmCpuManager.hpp"
#include <sys/time.h>
#include "defines.h"

#define N_PE 4
#define N_PE_X 2
#define N_PE_Y 2

#define SIM_ATTRS (ICM_ATTR_DEFAULT)

icmProcessorP processors[N_PE];
icmBusP bus[N_PE];
icmMemoryP mem[N_PE];
int dma_size[N_PE];
int dma_address[N_PE];
int dma_op[N_PE];
int dma_start[N_PE];
int header_dma_size[N_PE];
int data_in[N_PE];
int first_read[N_PE];
int page[N_PE];
int irq_mask[N_PE];
int irq_status[N_PE];
int counter_reg[N_PE];
int repo_write[N_PE];
int next_page[N_PE];
int first_address[N_PE];
int packets_in[N_PE];
int packets_out[N_PE];
int cpu_halt[N_PE];
icmPseP pse[N_PE];
icmPseP dynamicApps;
icmNetP router_address[N_PE];
icmNetP end_sim[N_PE];
icmNetP data_read[N_PE][NPORT];
icmNetP data_write[N_PE][NPORT];
icmNetP ni_intr[N_PE];
icmNetP timeslice_intr[N_PE];
icmNetP n_req_app;
icmNetP n_ack_app;
icmNetP n_app_size;
icmNetP n_finished_tasks;
int timeslice[N_PE];
int wait_scheduler[N_PE];
int scheduler[N_PE];
int ni_i[N_PE];
int timeslice_i[N_PE];
int req_app;
int ack_app;
int app_size;
float totalEnergy;
int totalVolume;
int volume[N_PE];
float energy[N_PE];
int router_cycles[N_PE];
int swap_read[N_PE];
int init_counter;
int mapped_tasks = 0;
int finished_tasks = 0;

struct timeval stop, start;
double max_instructions = 0;
double totalSimulated;
double elapsed;
double mips;

//function that converts big endian/little endian or little endian/big endian
Uns32 swap_Uns32(Uns32 val)
{
    val = ((val << 8) & 0xFF00FF00 ) | ((val >> 8) & 0xFF00FF );
    return (val << 16) | (val >> 16);
}

char nameString[64]; // temporary string to create labels

struct node {
  int x;
  struct node *next;
};

struct node *buff[N_PE];
struct node *first[N_PE];
struct node *clean[N_PE];
int size[N_PE];
int size_packet[N_PE];
int counter_packet[N_PE];
int counter[N_PE];
int counter_send[N_PE];
int size_send[N_PE];
int xo[N_PE];
int yo[N_PE];
int xd[N_PE];
int yd[N_PE];
int hops[N_PE];
int total_hops;

NET_WRITE_FN(req_app_update)
{
	req_app = value;
}

NET_WRITE_FN(app_size_update)
{
	app_size = value;
}

void bufferReceive(int p, int value)
{
		counter[p]++;
		if(first[p] == NULL)
		{
			first[p] = malloc( sizeof(struct node) );
			first[p]->x = value;
			first[p]->next = NULL;
			buff[p] = first[p];
		}
		else
		{
			buff[p]->next = malloc( sizeof(struct node) );
			buff[p] = buff[p]->next;
			buff[p]->x = value;
			buff[p]->next = NULL;
		}
		if(counter[p]==2)
		{
			size[p] = value;
		}
		if(counter[p]>2)
		{
			size[p]--;
			if(size[p]==0)
			{
				counter[p] = 0;
				packets_in[p]++;
			}
		}
}

NET_WRITE_FN(Receive)
{
	int processorNumber = (int) userData;
	bufferReceive(processorNumber, value);
}


int RouterPosition(int router)
{
	int pos;

	int column = router%N_PE_X;

	if(router>=(N_PE-N_PE_X)){ //TOP
		if(column==(N_PE_X-1)){ //RIGHT
			pos = TR;
		}
		else{
			if(column==0){//LEFT
				pos = TL;
			}
			else{//CENTER_X
				pos = TC;
			}
		}
	}
	else{
		if(router<N_PE_X){ //BOTTOM
			if(column==(N_PE_X-1)){ //RIGHT
				pos = BR;
			}
			else{
				if(column==0){//LEFT
					pos = BL;
				}
				else{//CENTER_X
					pos = BC;
				}
			}
		}
		else{//CENTER_Y
			if(column==(N_PE_X-1)){ //RIGHT
				pos = CRX;
			}
			else{
				if(column==0){//LEFT
					pos = CL;
				}
				else{//CENTER_X
					pos = CC;
				}
			}
		}
	}

	return pos;
}

int RouterPorts(int router)
{
	int ports;

	int column = router%N_PE_X;

	if(router>=(N_PE-N_PE_X)){ //TOP
		if(column==(N_PE_X-1)){ //RIGHT
			ports = 3;
		}
		else{
			if(column==0){//LEFT
				ports = 3;
			}
			else{//CENTER_X
				ports = 4;
			}
		}
	}
	else{
		if(router<N_PE_X){ //BOTTOM
			if(column==(N_PE_X-1)){ //RIGHT
				ports = 3;
			}
			else{
				if(column==0){//LEFT
					ports = 3;
				}
				else{//CENTER_X
					ports = 4;
				}
			}
		}
		else{//CENTER_Y
			if(column==(N_PE_X-1)){ //RIGHT
				ports = 4;
			}
			else{
				if(column==0){//LEFT
					ports = 4;
				}
				else{//CENTER_X
					ports = 5;
				}
			}
		}
	}

	return ports;
}

int RouterAddress(int router)
{
	int r_address, pos_x, pos_y;

	if(N_PE_X <= N_PE_Y)
	{
		pos_x = (router%N_PE_Y) << 8;
		pos_y = router/N_PE_Y;
	}
	else
	{
		pos_x = (router%N_PE_X) << 8;
		pos_y = router/N_PE_X;
	}

	r_address = pos_x | pos_y;
	return r_address;
}

string ftoa (float val)
{
  stringstream ss (stringstream::in | stringstream::out);
  ss << val;
  string valstr = ss.str();
  return valstr;
}

void update_volume(int xi, int yi, int xf, int yf, int vol)
{
	int xx = xi;
	int yy = yi;
	while(1)
	{
		if(xf>xi)
		{
			xx++;
		}
		if(xf<xi)
		{
			xx--;
		}
		if(xf==xi)
		{
			if(yf>yi)
			{
				yy++;
			}
			if(yf<yi)
			{
				yy--;
			}
		}
		xi = xx;
		yi = yy;
		if((xi==xf) && (yi==yf)) break;
	}
}

void dmaProcess(int p)
{
	int data[N_PE], verify_data[N_PE];
	while(dma_size[p])
	{
		if(dma_op[p] == READ)
		{
			if(repo_write[p]==0)
			{
				icmReadProcessorMemory(processors[p], dma_address[p], &data[p], 4);
				dma_address[p] = dma_address[p] + 4;
				counter_send[p]++;
				if(counter_send[p]==1)
				{
					xd[p] = swap_Uns32(data[p]) >> 8;
					yd[p] = swap_Uns32(data[p]) & 0xFF;

					xo[p] = RouterAddress(p) >> 8;
					yo[p] = RouterAddress(p) & 0xFF;

					hops[p] = (abs(xd[p]-xo[p])+abs(yd[p]-yo[p]));
				}
				if(counter_send[p]==2)
				{
					size_send[p] = swap_Uns32(data[p]);
					totalVolume = totalVolume + size_send[p] + 2;
					totalEnergy = totalEnergy + hops[p]*(size_send[p] + 2)*0.0035104;
					volume[p] = volume[p] + size_send[p] + 2;
					energy[p] = energy[p] + (size_send[p] + 2)*0.0035104;
					router_cycles[p] = router_cycles[p] + (size_send[p] + 5);
					total_hops = total_hops + hops[p];
					update_volume(xo[p], yo[p], xd[p], yd[p], size_send[p]+2);
				}
				if(counter_send[p]==3 && swap_Uns32(data[p])==0x00000040)
				{
					mapped_tasks++;
				}
				if(counter_send[p]==3 && swap_Uns32(data[p])==0x00000070)
				{
					mapped_tasks--;
					finished_tasks++;
					icmWriteNet(n_finished_tasks, finished_tasks);
				}
				if(counter_send[p]>2)
				{
					size_send[p]--;
					if(size_send[p]==0)
					{
						counter_send[p] = 0;
					}
				}
			}
			else
			{
				dma_address[p] = dma_address[p] + 1;
				counter_send[p]++;
				if(counter_send[p]==2)
				{
					size_send[p] = swap_Uns32(data[p]);
				}
				if(counter_send[p]>2)
				{
					size_send[p]--;
					if(size_send[p]==0)
					{
						counter_send[p] = 0;
					}
				}
			}
			icmWriteNet(data_read[p][LOCAL], swap_Uns32(data[p]));
			dma_size[p]--;
		}
		else
		{
			if(first[p] != NULL)
			{
				if(first_read[p] == 1)
				{
					free(clean[p]);
				}
				else
				{
					first_read[p] = 1;
				}
				data[p] = first[p]->x;
				if(page[p]!=0)
				{
					if(dma_address[p]==first_address[p])
					{
						data[p] = 0X3c1d0000 | (page[p]>>16);
						data[p] = swap_Uns32(data[p]);
					}
					if(dma_address[p]==(first_address[p]+4))
					{
						data[p] = 0X37bd0000 | ((page[p] & 0xFFFF)+0x4000);
						data[p] = swap_Uns32(data[p]);
					}
					if(dma_address[p]>(first_address[p]+4))
					{
						verify_data[p] = swap_Uns32(data[p]) >> 26;
						if(verify_data[p] == 3 || verify_data[p] == 2)
						{
							data[p] = swap_Uns32(data[p]);
							data[p] = data[p] | (page[p]>>2);
							data[p] = swap_Uns32(data[p]);
						}
					}
				}
				else data[p] = swap_Uns32(data[p]);
				icmWriteProcessorMemory(processors[p], dma_address[p], &data[p], 4);
				counter_packet[p]++;
				if(counter_packet[p] == 2)
				{
					size_packet[p] = data[p];
				}
				if(counter_packet[p] > 2)
				{
					size_packet[p]--;
					if(size_packet[p] == 0)
					{
						counter_packet[p]=0;
						packets_out[p]++;
					}
				}
				dma_address[p] = dma_address[p] + 4;
				dma_size[p]--;
				clean[p] = first[p];
				first[p] = first[p]->next;
			}
			else
			{
				first_read[p] = 0;
			}
		}
	}
	if(dma_size[p] == 0) dma_start[p] = 0;
}

//function acessed when a register is read from memory
int register_bank_read(int address, int p)
{
	int return_data;

	switch(address)
	{
		case DMA_READY:
			if(dma_start[p] == 1)
			{
				return_data = dma_size[p];
			}
			else
			{
				return_data = 0;
			}
		break;

		case PROC_ADDRESS:
			return_data = swap_Uns32(RouterAddress(p));
		break;

		case START_SYSTEM:
			if(init_counter!=N_PE)
			{
				return_data = 1;
			}
			else
			{
				return_data = 0;
			}
		break;

		case READ_NI_READY:
			if(first[p] != NULL)
			{
				if(first_read[p] == 1)
				{
					free(clean[p]);
				}
				else
				{
					first_read[p] = 1;
				}
				data_in[p] = swap_Uns32(first[p]->x);
				counter_packet[p]++;
				if(counter_packet[p] == 2)
				{
					size_packet[p] = first[p]->x;
				}
				if(counter_packet[p] > 2)
				{
					size_packet[p]--;
					if(size_packet[p] == 0)
					{
						counter_packet[p]=0;
						packets_out[p]++;
					}
				}
				header_dma_size[p]--;
				clean[p] = first[p];
				first[p] = first[p]->next;
			}
			else
			{
				first_read[p] = 0;
			}
			return_data = header_dma_size[p];
		break;

		case DATA_IN:
			if(swap_read[p]==0)
			{
				return_data = data_in[p];
			}
			else
			{
				return_data = swap_Uns32(data_in[p]);
			}
		break;

		case IRQ_STATUS:
			if(ni_i[p]==1 && timeslice_i[p]==1)
			{
				return swap_Uns32(IRQ_NOC|IRQ_COUNTER18);
			}
			else
			{
				if(ni_i[p]==1) return swap_Uns32(IRQ_NOC);
				if(timeslice_i[p] ==1) return swap_Uns32(IRQ_COUNTER18);
			}
		break;

		case REQ_APP:
			return_data = swap_Uns32(req_app);
		break;

		case APP_SIZE:
			return_data = swap_Uns32(app_size);
		break;

		default: break;
	}
	return return_data;
}

//function acessed when a register is written in memory
void register_bank_write(int address, int value, int p)
{
	int x, y;

	switch(address)
	{
		case DMA_SIZE:
			dma_size[p] = value;
		break;

		case DMA_ADDRESS:
			dma_address[p] = value;
		break;

		case DMA_OP:
			dma_op[p] = value;
		break;

		case DMA_START:
			first_address[p] = dma_address[p];
			dma_start[p] = 1;
			dmaProcess(p);
		break;

		case START_READ_NI:
			header_dma_size[p] = 1;
		break;

		case WRITE_NI:
			icmWriteNet(data_read[p][LOCAL], value);
			counter_send[p]++;
			if(counter_send[p]==1)
			{
				xd[p] = value >> 8;
				yd[p] = value & 0xFF;

				xo[p] = RouterAddress(p) >> 8;
				yo[p] = RouterAddress(p) & 0xFF;

				hops[p] = (abs(xd[p]-xo[p])+abs(yd[p]-yo[p]));
			}
			if(counter_send[p]==2)
			{
				size_send[p] = value;
			}
			if(counter_send[p]>2)
			{
				size_send[p]--;
				if(size_send[p]==0)
				{
					counter_send[p] = 0;
				}
			}
		break;

		case PAGE:
			page[p] = value;
		break;

		case IRQ_MASK:
			irq_mask[p] = value;
		break;

		case COUNTER_REG:
			counter_reg[p] = value;
		break;

		case REPOSITORY:
			repo_write[p] = value;
		break;

		case SWAP_READ:
			swap_read[p] = value;
		break;

		case NEXT_PAGE:
			counter_reg[p] = 0;
			next_page[p] = value;
		break;

		case SCHEDULER_CALLED:
			scheduler[p] = 1;
		break;

		case ACK_APP:
			icmWriteNet(n_ack_app, value);
		break;

		case INITIALIZE_ROUTER:
			icmWriteNet(router_address[value], RouterAddress(value));
		break;

	default: break;
	}
}

ICM_MEM_WATCH_FN(sendRegData)
{
	int p = atoi(icmGetProcessorName(processor, "\n"));
	if(packets_in[p]!=packets_out[p])
	{
		ni_i[p] = 1;
		icmWriteNet(ni_intr[p], 1);
	}
	else
	{
		ni_i[p] = 0;
		icmWriteNet(ni_intr[p], 0);
	}

	if(next_page[p] != 0)
	{
		if(wait_scheduler[p]==1)
		{
			if(scheduler[p]==1)
			{
				scheduler[p]=0;
				wait_scheduler[p]=0;
				timeslice_i[p] = 0;
				icmWriteNet(timeslice_intr[p], 0);
			}
			else
			{
				timeslice_i[p] = 1;
				icmWriteNet(timeslice_intr[p], 1);
			}

		}
		else
		{
			if(timeslice[p]>=16384)
			{
				wait_scheduler[p] = 1;
				timeslice[p] = 0;
			}
			else
			{
				timeslice[p]++;
			}
		}
	}
	else
	{
		timeslice[p] = 0;
	}
}

//register bank callback functions
ICM_MEM_READ_FN(regbank_rb)
{
	int p = atoi(icmGetProcessorName(processor, "\n"));
	int data = register_bank_read(address, p);
	*(Int32 *)value = data;
}

ICM_MEM_WRITE_FN(regbank_wb)
{
	int p = atoi(icmGetProcessorName(processor, "\n"));
	register_bank_write(address, swap_Uns32(*(Uns32*)value), p);
}

//pagination callback functions
ICM_MEM_READ_FN(mem_rb)
{
	int new_address, data;
	int p = atoi(icmGetProcessorName(processor, "\n"));

	if(next_page[p]!=0)
	{
		new_address = (Int32)address | next_page[p];
		icmReadProcessorMemory(processor, new_address, &data, 4);
		*(Int32 *)value = data;
	}
}

ICM_MEM_WRITE_FN(mem_wb)
{
	int new_address;
	int p = atoi(icmGetProcessorName(processor, "\n"));

	new_address = (Int32)address | next_page[p];
	icmWriteProcessorMemory(processor, new_address, value, 4);
}

//
// Main routine
//
int main(int argc, char **argv) {

	gettimeofday(&start, NULL);//----------------------------------------------

    // initialize OVPsim, enabling verbose mode to get statistics at end
    // of execution
    icmInit(ICM_VERBOSE|ICM_STOP_ON_CTRLC, 0, 0);

    const char *model       = icmGetVlnvString(NULL, "mips.ovpworld.org", "processor", "mips32", "1.0", "model");
	const char *semihosting = icmGetVlnvString(NULL, "mips.ovpworld.org", "semihosting", "mips32SDE", "1.0", "model");

	//create a user attribute object
	icmAttrListP userAttrs = icmNewAttrList();
	//add a double attribute to set mips to 1000
	icmAddDoubleAttr(userAttrs, "mips", 1000.0);
    // Create the processor instances
    int stepIndex;
    for (stepIndex=0; stepIndex < N_PE; stepIndex++)
	 {
		sprintf(nameString, "%d", stepIndex);
        processors[stepIndex] = icmNewProcessor(
            nameString,            // CPU name
            "mips32",           // CPU type
            stepIndex,          // CPU cpuId
            0,                  // CPU model flags
            32,                 // address bits
            model,        // model file
            "modelAttrs",       // morpher attributes
            SIM_ATTRS,          // attributes
            0,          // user-defined attributes
            semihosting,     // semi-hosting file
            "modelAttrs"        // semi-hosting attributes
        );

        // create the processor bus
        sprintf(nameString, "BUS-%d", stepIndex);
        bus[stepIndex] = icmNewBus(nameString, 32);

        // connect the processors onto the busses
        icmConnectProcessorBusses(processors[stepIndex], bus[stepIndex], bus[stepIndex]);

        // create memory
        sprintf(nameString, "MEM-%d", stepIndex);
        mem[stepIndex] = icmNewMemory(nameString, ICM_PRIV_RWX, 0xefffffff);

        // connect the memory onto the busses
        icmConnectMemoryToBus(bus[stepIndex], "mp1", mem[stepIndex], 0x00000000);
        sprintf(nameString, "extMemReg-%d", stepIndex);
        icmMapExternalMemory(bus[stepIndex], nameString, ICM_PRIV_RW, 0xf0000000, 0xffffffff, regbank_rb, regbank_wb, 0);

        sprintf(nameString, "router-%d", stepIndex);
        pse[stepIndex] = icmNewPSE(nameString, "/home/madalozzo/Desktop/ovp/platform_ovp/router/pse.pse", NULL, NULL, NULL);

        sprintf(nameString, "router_address-%d", stepIndex);
		router_address[stepIndex] = icmNewNet(nameString);
		icmConnectPSENet(pse[stepIndex], router_address[stepIndex], "router_address", ICM_INPUT);

        sprintf(nameString, "end_sim-%d", stepIndex);
		end_sim[stepIndex] = icmNewNet(nameString);
		icmConnectPSENet(pse[stepIndex], end_sim[stepIndex], "end_sim", ICM_INPUT);

        sprintf(nameString, "read_data-%d-local", stepIndex);
		read_data[stepIndex][LOCAL] = icmNewNet(nameString);
		icmConnectPSENet(pse[stepIndex], read_data[stepIndex][LOCAL], "read_data_4", ICM_INPUT);

		sprintf(nameString, "data_read-%d-local", stepIndex);
		data_read[stepIndex][LOCAL] = icmNewNet(nameString);
		icmConnectPSENet(pse[stepIndex], data_read[stepIndex][LOCAL], "data_read_4", ICM_INPUT);

		sprintf(nameString, "send_data-%d-local", stepIndex);
        send_data[stepIndex][LOCAL] = icmNewNet(nameString);
        icmConnectPSENet(pse[stepIndex], send_data[stepIndex][LOCAL], "send_data_4", ICM_OUTPUT);

        sprintf(nameString, "data_write-%d-local", stepIndex);
        data_write[stepIndex][LOCAL] = icmNewNet(nameString);
        icmConnectPSENet(pse[stepIndex], data_write[stepIndex][LOCAL], "data_write_4", ICM_OUTPUT);
	 }
    for (stepIndex=0; stepIndex < N_PE; stepIndex++)
    {
		printf("RouterAddress(%d) 0x%02x\n", stepIndex, RouterAddress(stepIndex));

		//EAST CONNECTION
		if(RouterPosition(stepIndex) != BR && RouterPosition(stepIndex) != CRX && RouterPosition(stepIndex) != TR)
		{
			//printf("EAST CONNECTION\n");
			sprintf(nameString, "data_read-%d-east", stepIndex);
			data_read[stepIndex][EAST] = icmNewNet(nameString);
			icmConnectPSENet(pse[stepIndex], data_read[stepIndex][EAST], "data_read_0", ICM_INPUT);
			icmConnectPSENet(pse[stepIndex+1], data_read[stepIndex][EAST], "data_write_1", ICM_OUTPUT);
		}

		//WEST CONNECTION
		if(RouterPosition(stepIndex) != BL && RouterPosition(stepIndex) != CL && RouterPosition(stepIndex) != TL)
		{
			//printf("WEST CONNECTION\n");
			sprintf(nameString, "data_read-%d-west", stepIndex);
			data_read[stepIndex][WEST] = icmNewNet(nameString);
			icmConnectPSENet(pse[stepIndex], data_read[stepIndex][WEST], "data_read_1", ICM_INPUT);
			icmConnectPSENet(pse[stepIndex-1], data_read[stepIndex][WEST], "data_write_0", ICM_OUTPUT);
		}

		//NORTH CONNECTION
		if(RouterPosition(stepIndex) != TL && RouterPosition(stepIndex) != TC && RouterPosition(stepIndex) != TR)
		{
			//printf("NORTH CONNECTION\n");
			sprintf(nameString, "data_read-%d-north", stepIndex);
			data_read[stepIndex][NORTH] = icmNewNet(nameString);
			icmConnectPSENet(pse[stepIndex], data_read[stepIndex][NORTH], "data_read_2", ICM_INPUT);
			icmConnectPSENet(pse[stepIndex+N_PE_X], data_read[stepIndex][NORTH], "data_write_3", ICM_OUTPUT);
		}

		//SOUTH CONNECTION
		if(RouterPosition(stepIndex) != BL && RouterPosition(stepIndex) != BC && RouterPosition(stepIndex) != BR)
		{
			//printf("SOUTH CONNECTION\n");
			sprintf(nameString, "data_read-%d-south", stepIndex);
			data_read[stepIndex][SOUTH] = icmNewNet(nameString);
			icmConnectPSENet(pse[stepIndex], data_read[stepIndex][SOUTH], "data_read_3", ICM_INPUT);
			icmConnectPSENet(pse[stepIndex-N_PE_X], data_read[stepIndex][SOUTH], "data_write_2", ICM_OUTPUT);
		}
		icmAddNetCallback(data_write[stepIndex][LOCAL], Receive, (void*) stepIndex);
	}

	for(stepIndex=0; stepIndex < N_PE; stepIndex++) {
		if(stepIndex==0) icmLoadProcessorMemory(processors[stepIndex], "/home/madalozzo/Desktop/ovp/bareMetal/fibonacci0.MIPS32.elf", 0, 0, 1);
		else if(stepIndex==1) icmLoadProcessorMemory(processors[stepIndex], "/home/madalozzo/Desktop/ovp/bareMetal/fibonacci1.MIPS32.elf", 0, 0, 1);
		else if(stepIndex==2) icmLoadProcessorMemory(processors[stepIndex], "/home/madalozzo/Desktop/ovp/bareMetal/fibonacci2.MIPS32.elf", 0, 0, 1);
		else icmLoadProcessorMemory(processors[stepIndex], "/home/madalozzo/Desktop/ovp/bareMetal/fibonacci3.MIPS32.elf", 0, 0, 1);
	}

    // simulate the platform
    icmProcessorP final = icmSimulatePlatform();

	// was simulation interrupted or did it complete
    if(final && (icmGetStopReason(final)==ICM_SR_INTERRUPT)) {
        icmPrintf("*** simulation interrupted\n");
    }

    icmTerminate();

    return 0;
}

