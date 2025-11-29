/**
 * @file interrupts_EP_DanielKuchanski_JacobGaumond.cpp
 * @author Daniel Kuchanski
 * @author Jacob Gaumond
 * @brief main.cpp file of system with external priorities (no preemption) scheduling for Assignment 3 Part 1 of SYSC4001
 * 
 */

#include<interrupts_DanielKuchanski_JacobGaumond.hpp>

void FCFS(std::vector<PCB> &ready_queue) {
    std::sort( 
                ready_queue.begin(),
                ready_queue.end(),
                []( const PCB &first, const PCB &second ){
                    return (first.arrival_time > second.arrival_time); 
                } 
            );
}

std::tuple<std::string /* add std::string for bonus mark */ > run_simulation(std::vector<PCB> list_processes) {

    std::vector<PCB> ready_queue;   //The ready queue of processes
    std::vector<PCB> wait_queue;    //The wait queue of processes
    std::vector<PCB> job_list;      //A list to keep track of all the processes. This is similar
                                    //to the "Process, Arrival time, Burst time" table that you
                                    //see in questions. You don't need to use it, I put it here
                                    //to make the code easier :).

    unsigned int current_time = 0;
    PCB running;

    //Initialize an empty running process
    idle_CPU(running);

    std::string execution_status;

    //make the output table (the header row)
    execution_status = print_exec_header();

    //Loop while till there are no ready or waiting processes.
    //This is the main reason I have job_list, you don't have to use it.
    while(!all_process_terminated(job_list) || job_list.empty()) {

        //Terminate running if finished
        if(running.state != NOT_ASSIGNED) {
            if(((current_time - running.start_time) - running.remaining_time) == 0) {
                running.state = TERMINATED;
                running.remaining_time -= current_time - running.start_time;
                running.exe_time_without_io += current_time - running.start_time;
                running.start_time = -1;
                sync_queue(job_list, running);

                free_memory(running);
                idle_CPU(running);
            }
        }

        //Inside this loop, there are three things you must do:
        // 1) Populate the ready queue with processes as they arrive
        // 2) Manage the wait queue
        // 3) Schedule processes from the ready queue

        //Populate the ready queue with processes as they arrive
        for(auto &process : list_processes) {
            if(process.arrival_time == current_time) {//check if the AT = current time
                //if so, change the process's state to NEW
                process.state = NEW; //Set the process state to NEW
                job_list.push_back(process); //Add it to the list of processes

                execution_status += print_exec_status(current_time, process.PID, NOT_ASSIGNED, NEW);
            }
        }

        //Attempt to assign memory in the order of arrival (using job_list)
        for(auto &process : job_list) {
            if(process.state == NEW) { //Only assign memory to unadmitted processes
                if(assign_memory(process)) {
                    //If memory assigned successfully, put the process into the ready queue and update its state
                    process.state = READY; //Set the process state to READY
                    sync_queue(job_list, process);
                    ready_queue.push_back(process); //Add the process to the ready queue

                    execution_status += print_exec_status(current_time, process.PID, NEW, READY);
                }
            }
        }

        ///////////////////////MANAGE WAIT QUEUE/////////////////////////
        //This mainly involves keeping track of how long a process must remain in the ready queue

        //If the current RUNNING process is due for I/O, put it in the waiting queue
        if(running.state != NOT_ASSIGNED) {
            if(((current_time - running.start_time) + running.exe_time_without_io) == running.io_freq) {
                running.state = WAITING;
                running.remaining_time -= current_time - running.start_time;
                running.exe_time_without_io += current_time - running.start_time;
                running.start_time = -1;
                sync_queue(job_list, running);
                wait_queue.push_back(running);

                execution_status += print_exec_status(current_time, running.PID, RUNNING, WAITING);

                idle_CPU(running);
            }
        }

        //If I/O is available (it always is), send wait_queue processes to I/O
        while(wait_queue.size() > 0) {
            PCB waiting_process = dequeue_process(wait_queue);

            waiting_process.exe_time_without_io = 0;
            waiting_process.start_time = current_time;
            sync_queue(job_list, waiting_process);
        }

        //If I/O is complete, send WAITING processes to the ready queue
        for(auto &process : job_list) {
            if(process.state == WAITING) {
                if((current_time - process.start_time) == process.io_duration) {
                    process.state = READY;
                    process.start_time = -1;
                    sync_queue(job_list, process);
                    ready_queue.push_back(process);

                    execution_status += print_exec_status(current_time, process.PID, WAITING, READY);
                }
            }
        }

        /////////////////////////////////////////////////////////////////

        //////////////////////////SCHEDULER//////////////////////////////
        FCFS(ready_queue); //example of FCFS is shown here
        /////////////////////////////////////////////////////////////////

        //End each cycle by incrementing the clock
        current_time += 1;
    }
    
    //Close the output table
    execution_status += print_exec_footer();

    return std::make_tuple(execution_status);
}


int main(int argc, char** argv) {

    //Get the input file from the user
    if(argc != 2) {
        std::cout << "ERROR!\nExpected 1 argument, received " << argc - 1 << std::endl;
        std::cout << "To run the program, do: ./interrutps <your_input_file.txt>" << std::endl;
        return -1;
    }

    //Open the input file
    auto file_name = argv[1];
    std::ifstream input_file;
    input_file.open(file_name);

    //Ensure that the file actually opens
    if (!input_file.is_open()) {
        std::cerr << "Error: Unable to open file: " << file_name << std::endl;
        return -1;
    }

    //Parse the entire input file and populate a vector of PCBs.
    //To do so, the add_process() helper function is used (see include file).
    std::string line;
    std::vector<PCB> list_process;
    while(std::getline(input_file, line)) {
        auto input_tokens = split_delim(line, ", ");
        auto new_process = add_process(input_tokens);
        list_process.push_back(new_process);
    }
    input_file.close();

    //With the list of processes, run the simulation
    auto [exec] = run_simulation(list_process);

    write_output(exec, "execution.txt");

    return 0;
}
