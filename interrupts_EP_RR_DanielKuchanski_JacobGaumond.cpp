/**
 * @ file interrupts_EP_RR_DanielKuchanski_JacobGaumond.cpp
 * @ author Daniel Kuchanski
 * @ author Jacob Gaumond
 * @ brief main.cpp file of system with external priorities(with premption and a 100ms timeout, in round - robin fashion) scheduling for Assignment 3 Part 1 of SYSC4001
 *
 */

# include "interrupts_DanielKuchanski_JacobGaumond.hpp"

void FCFS(std::vector<PCB> &ready_queue) {
    std::sort( 
                ready_queue.begin(),
                ready_queue.end(),
                []( const PCB &first, const PCB &second ){
                    return (first.arrival_time < second.arrival_time); 
                } 
            );
}

// Quantum for Round Robin in milliseconds
constexpr unsigned int RR_QUANTUM = 100;

// Helper: produce a memory snapshot string appended to execution_status when a process is admitted
static std::string memory_snapshot_string() {
    std::stringstream ss;
    unsigned int total_used = 0;
    unsigned int total_free = 0;
    unsigned int usable_free = 0;

    ss << "\nMemory Snapshot:\n";

    // Print partition usage
    ss << "Partitions: ";
    for (int i = 0; i < 6; ++i) {
        ss << "[" << memory_paritions[i].partition_number << ": " 
           << memory_paritions[i].size << "MB ";
        if (memory_paritions[i].occupied == -1) {
            ss << "FREE";
            total_free += memory_paritions[i].size;
            usable_free += memory_paritions[i].size;
        } else {
            ss << "USED(PID=" << memory_paritions[i].occupied << ")";
            total_used += memory_paritions[i].size;
        }
        ss << "]";
        if (i < 5) ss << " ";
    }
    ss << "\nTotal memory used: " << total_used << " MB\n";
    ss << "Total free memory: " << total_free << " MB\n";
    ss << "Total usable memory (sum of free partitions): " << usable_free << " MB\n\n";

    return ss.str();
}

// pick index in ready_queue of highest priority (lowest PID) process (returns -1 if none)
static int pick_highest_priority_index(const std::vector<PCB> &ready_queue) {
    if (ready_queue.empty()) return -1;
    int best_idx = 0;
    for (size_t i = 1; i < ready_queue.size(); ++i) {
        if (ready_queue[i].PID < ready_queue[best_idx].PID) {
            best_idx = static_cast<int>(i);
        } else if (ready_queue[i].PID == ready_queue[best_idx].PID) {
            // tie-breaker: earlier arrival_time wins
            if (ready_queue[i].arrival_time < ready_queue[best_idx].arrival_time) {
                best_idx = static_cast<int>(i);
            }
        }
    }
    return best_idx;
}

std::tuple<std:: string /* add std: : string for bonus mark */ > run_simulation(std::vector<PCB> list_processes) {

    std::vector<PCB> ready_queue;   // The ready queue of processes
    std::vector<PCB> wait_queue;    // The wait queue of processes
    std::vector<PCB> job_list;      // A list to keep track of all the processes. This is similar 
                                    // to the "Process, Arrival time, Burst time" table that you 
                                    // see in questions. You don't need to use it, I put it here 
                                    // to make the code easier : ).

    unsigned int current_time = 0;
    PCB running;

    //Initialize an empty running process
    idle_CPU(running);
    
    
    //make the output table(the header row)
    std::string execution_status = print_exec_header();
    
    unsigned int time_slice_remaining = 0;

    const size_t total_processes = list_processes.size();
    size_t terminated_count = 0;

    //Loop while till there are no ready or waiting processes.
    //This is the main reason I have job_list, you don't have to use it.
    while(terminated_count < total_processes) {
        
        //Populate the ready queue with processes as they arrive
        for(auto &process : list_processes) {
            if(process.arrival_time == current_time) {//check if the AT = current time
                //if so, change the process's state to NEW
                process.state = NEW; //Set the process state to NEW
                job_list.push_back(process); //Add it to the list of processes
            }
        }
        
        //Attempt to assign memory in the order of arrival (using job_list)
        for(auto &process : job_list) {
            if(process.state == NEW) { //Only assign memory to unadmitted processes
                if(assign_memory(process)) {
                    //If memory assigned successfully, put the process into the ready queue and update its state
                    process.state = READY; //Set the process state to READY
                    process.start_time = -1;
                    sync_queue(job_list, process);
                    ready_queue.push_back(process); //Add the process to the ready queue
                    execution_status += print_exec_status(current_time, process.PID, NEW, READY);
                }
            }
        }

        // Terminate running if finished
        if(running.state != NOT_ASSIGNED) {
            // Increment execution times for running process for this 1ms tick
            running.remaining_time = (running.remaining_time > 0) ? running.remaining_time - 1 : 0;
            running.exe_time_without_io += 1;
            if (time_slice_remaining > 0) time_slice_remaining -= 1;

            if (running.remaining_time == 0 && running.state == RUNNING) {
                states old_state = running.state;
                running.state = TERMINATED;
                running.start_time = -1;
                sync_queue(job_list, running);

                // free memory & update counts
                free_memory(running);
                terminated_count += 1;
                execution_status += print_exec_status(current_time, running.PID, old_state, TERMINATED);

                // clear CPU
                idle_CPU(running);
                time_slice_remaining = 0;
            }
            // Check if time slice expired
            else if (time_slice_remaining == 0 && running.state == RUNNING) {
                // preempt only if it still needs CPU
                if (running.remaining_time > 0) {
                    states old_state = running.state;
                    running.state = READY;
                    running.start_time = -1;
                    sync_queue(job_list, running);
                    ready_queue.push_back(running); // enqueue at back for RR

                    execution_status += print_exec_status(current_time, running.PID, old_state, READY);

                    // clear CPU
                    idle_CPU(running);
                }
            }
        }
        //Inside this loop, there are three things you must do: 
        // 1) Populate the ready queue with processes as they arrive
        // 2) Manage the wait queue 
        // 3) Schedule processes from the ready queue


        ///////////////////////MANAGE WAIT QUEUE/////////////////////////
        //This mainly involves keeping track of how long a process must remain in the ready queue

        //If the current RUNNING process is due for I/O, put it in the waiting queue
        if (running.io_freq > 0 && running.exe_time_without_io >= running.io_freq && running.remaining_time > 0) {
                states old = running.state;
                running.state = WAITING;
                running.start_time = current_time;
                running.exe_time_without_io = 0;
                sync_queue(job_list, running);
                wait_queue.push_back(running);
            
                execution_status += print_exec_status(current_time, running.PID, old, WAITING);
            
                idle_CPU(running);
                time_slice_remaining = 0;
        }

        //If I/O is available (it always is), send wait_queue processes to I/O
        while(wait_queue.size() > 0) {
            PCB waiting_process = dequeue_process(wait_queue);
            for (auto &proc : job_list) {
                if (proc.PID == waiting_process.PID) {
                    proc.exe_time_without_io = 0;
                    proc.start_time = current_time; // I/O start
                    sync_queue(job_list, proc);
                    break;
                }
            }
        }

        //If I/O is complete, send WAITING processes to the ready queue
        for(auto &process : job_list) {
            if(process.state == WAITING) {
                if((current_time - process.start_time) == process.io_duration) {
                    states old = process.state;
                    process.state = READY;
                    process.start_time = -1;
                    sync_queue(job_list, process);
                    ready_queue.push_back(process);

                    execution_status += print_exec_status(current_time, process.PID, old, READY);
                }
            }
        }

        /////////////////////////////////////////////////////////////////

        //////////////////////////SCHEDULER//////////////////////////////
        if (running.state == NOT_ASSIGNED) {
            int idx = pick_highest_priority_index(ready_queue);
            if (idx != -1 && !ready_queue.empty()) {
                PCB selected = ready_queue[idx];
                std::vector<PCB> new_ready;
                for (size_t i = 0; i < ready_queue.size(); ++i) {
                    if (static_cast<int>(i) != idx) new_ready.push_back(ready_queue[i]);
                }
                ready_queue = std::move(new_ready);
                for (auto &proc : job_list) {
                    if (proc.PID == selected.PID) {
                        proc.state = RUNNING;
                        proc.start_time = current_time;
                        sync_queue(job_list, proc);
                        running = proc;
                        execution_status += print_exec_status(current_time, running.PID, READY, RUNNING);
                        time_slice_remaining = static_cast<unsigned int>( std::min<unsigned int>(RR_QUANTUM, running.remaining_time) );
                        break;
                    }
                }
            }
        }
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
        std::cout << "To run the program, do: ./interrupts <your_input_file.txt>" << std::endl;
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
        if(line.size() == 0) continue;
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
