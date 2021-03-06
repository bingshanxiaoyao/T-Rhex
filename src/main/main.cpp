/**
 * Main runner file for T-Rhex
 * 
 * Created by Vivaan Bahl, vrbahl
*/

#include <unistd.h>
#include <iostream>
#include <csignal>

#include "config/config.h"
#include "dynamixel_interface/dynamixel_interface.h"
#include "instruction_parser/instruction_parser.h"
#include "microcontroller/microcontroller.h"
#include "network_interface/network_interface.h"

static bool run_system = true;

void sigint_handler ( int signum )
{
    run_system = false;
    std::cout << "Stopping system..." << std::endl;
    return;

}

int main(int argc, char **argv)
{

    std::string gait_file;
    if (argc != 2 && argc != 3)
    {
        std::cerr << "Usage: ./trhex <gait_file> [--no-server]" << std::endl;
        return 1;
    }
    else
    {
        gait_file = std::string(argv[1]);
    }

    bool run_server = true;
    if (argc == 3)
    {
        std::string server_param = std::string(argv[2]);
        if (server_param == "--no-server")
        {
            run_server = false;
        }
        else
        {
            std::cerr << "Usage: ./trhex <gait_file> [--no-server]" << std::endl;
            return 1;
        }
    }
    

    signal(SIGINT, sigint_handler);

    InstructionParser instruction_parser(gait_file);
    std::vector<Instruction *> instruction_set = instruction_parser.get_instruction_set();

    std::cout << "Read instruction file" << std::endl;

    DynamixelInterface dynamixel_interface;
    Microcontroller microcontroller(instruction_set);
    NetworkInterface network_interface(run_server);

    Instruction *micr_dyni_current_instruction;
    bool micr_dyni_execute = false;

    bool dyni_micr_is_finished = false;
    uint16_t *dyni_micro_leg_data = (uint16_t *) malloc(sizeof(uint16_t) * NUM_LEGS);
    for (int i = 0; i < NUM_LEGS; i++)
    {
	    dyni_micro_leg_data[i] = 0;
    }

    std::cout << "Initialized" << std::endl;



#if VERBOSE
    int count = 0;
#endif

    while (run_system)
    {
        // MICROCONTROLLER
        // send inputs
        microcontroller.set_leg_data(dyni_micro_leg_data);
        microcontroller.set_curr_ins_finished(dyni_micr_is_finished);
	// tick
        microcontroller.tick();
        // get outputs
        micr_dyni_current_instruction = microcontroller.get_curr_inst();
        micr_dyni_execute = microcontroller.get_send_instr();

#if VERBOSE
	std::cout << "Micro tick" << std::endl;
#endif

        // DYNAMIXEL INTERFACE
        // send inputs
        dynamixel_interface.set_instr(micr_dyni_current_instruction);
        dynamixel_interface.set_run_command(micr_dyni_execute);
        // tick
        dynamixel_interface.tick();
        // get outputs
        dyni_micr_is_finished = dynamixel_interface.get_cmd_finished();
        dyni_micro_leg_data = dynamixel_interface.get_pos_data();

#if VERBOSE
	if (count % 1 == 0)
	{
		std::cout << "positions: ";
		for (int i = 0; i < 6; i++)
		{
			std::cout << " " << std::to_string(dyni_micro_leg_data[i]);
		}
		std::cout << std::endl;
		count = 0;
	}
	count++;
#endif

#if VERBOSE
	std::cout << "dyn tick" << std::endl;
#endif

        // NETWORK INTERFACE
        // send inputs
        network_interface.set_leg_data(dyni_micro_leg_data);
        network_interface.set_current_instruction(micr_dyni_current_instruction);
        // tick
        network_interface.tick();
        // get outputs

        usleep(500); // 0.5ms run loop
#if VERBOSE
        usleep(500 * 1000);
#endif
    }

    std::cout << "Stopping system" << std::endl;

    microcontroller.set_shutdown_cmd(true);
    dynamixel_interface.set_shutdown(true);

    while (dynamixel_interface.is_port_open())
    {
        microcontroller.tick();
        dynamixel_interface.tick();
        usleep(10 * 1000);
    }


    return 0;
}
