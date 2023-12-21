#include <cctype> // for toupper()
#include <cstdlib> // for EXIT_SUCCESS and EXIT_FAILURE
#include <cstring> // for strerror()
#include <cerrno> // for errno
#include <deque> // for deque (used for ready and blocked queues)
#include <fstream> // for ifstream (used for reading simulated programs)
#include <iostream> // for cout, endl, and cin
#include <sstream> // for stringstream (used for parsing simulated programs)
#include <sys/wait.h> // for wait()
#include <unistd.h> // for pipe(), read(), write(), close(), fork(), and _exit()
#include <vector> // for vector (used for PCB table) 
using namespace std;

/* 
Instruction class defintion --> an instance will feature 2 things
    1. A character signifying it's operation
        - ex) S, A, R, F, E, etc...
    2. An argument --> this will either be a String or Integer (it depends on the operation...)
Usages --> we read a series of instructions from files like init, which simulates a process carrying out a series of instructions
    - this class helps us work instructions more easily
*/
class Instruction {
    public:
        char operation;
        int intArg;
        string stringArg;
};

/*
Cpu class definition --> an instance will feature 3 things
    1. A vector of Instructions --> this vector will contain ALL the instructions for the currently running process
        - ex) if process 0's instructions were from the "init" file, then this vector would contain S 1000, A 19, etc...
    2. A integer corresponding to the programCounter of a given process
    3. An integer that we'll manipulate as we run through various processes
        - this value is initialized to 1000, and will be incremented, decremented, etc... by various processes' instructions
*/
class Cpu {
    public:
        vector<Instruction> *pProgram;
        int programCounter;
        int value;
};

/*
Here, we've defined 4 states in which a created process can be in
    1. STATE_READY --> the process is in a ready state but another process is being run
        - this means the process is currently in the waiting queue...
    2. STATE_RUNNING --> the process is currently being run by the CPU
    3. STATE_BLOCKED --> the process has been blocked by a 'B' instruction
        - to unblock, we must enter 'U' as a process simulator
    4. STATE_FINISHED --> the process has finished it's entire execution
        - a process will only be in this state if it has run an 'E' operation
            - an 'E' operation is run when either the process has an 'E' operation in it's list of operations, OR if the file containing the list of operations ends
*/
enum State {
    STATE_READY = 1,
    STATE_RUNNING,
    STATE_BLOCKED,
    STATE_FINISHED
};

/*
PcbEntry class definition --> an instance will feature various things
    1. processId --> this is the ID of the process itself...
        - in our simulation, this simply corresponds to the process # in order of creation (with 0 indexing)
            ex) our first ever process will have a processId of 0
            ex) our first ever fork will create a child process, which is our second process in our entire simulation; thus, it'll have a processId of 1 (due to 0-indexing)
    2. parentProcessId --> this is the ID of a process' parent if it has one
        - this will only be set if the process was created via a fork... so every process besides process 0
    3. program
        - this is a vector containing ALL the instructions that the current process has and will run
        - ex) Process 0's program vector will contain each instruction in the "init" file
    4. programCounter
        - this is the programCounter of a specific process --> this allows a process to keep track of where it is in it's own list of instructions,making sure that when context-switching occurs, when we return to a process, it continues its instruction(s) from where we last left off
    5. value
    6. state
        - this corresponds to the state of a given process --> we defined the four possible states (ready, running, blocked, finished) with an enum
    7. startTime
        - this corresponds to the startTime of a given process (used to calculate turnaround time)
    8. finishTime
        - this corresponds to the finishTime of a given process (used to calculate turnaround time)
- In short, a PcbEntry is just a process...
*/
class PcbEntry {
    public:
        int processId;
        int parentProcessId;
        vector<Instruction> program;
        unsigned int programCounter;
        int value;
        State state;
        unsigned int startTime;
        unsigned int finishTime;
};

/*
Let's setup our simulation --> here's a few important variables
    1. An array of PcbEntry
        - this will allow us to reference all of our processes
        - notice we fix the size to 10 --> for the purpose of this simulation, we'll have less than 10 processes anyway...
    2. timestamp
        - this is quite literally time --> at the start of our simulation, it'll be at time 0. this value will be incremented as we put in the 'Q' instruction, passing time quantum
    3. cpu
        - this will be our sole instance of the cpu in our simulation
    4. currentRunningProcessID
        - At all times, we'll have a reference to the ID of the current running process using this variable
        - since we now have an array of PcbEntry, the ID of a process corresponds to it's index in the array...
    5. readyState
        - queue containing processes in a ready state
    6. blockedState
        - queue containing processes in a blocked state
    7. numProcesses
        - a variable to keep track of the number of processes at a given moment
        - this allows us to create processes in our array of PcbEntry more easily because upon initialization
*/
PcbEntry pcbEntry[10];
unsigned int timestamp = 0;
Cpu cpu;
int currentRunningProcessID = -1;
deque<int> readyState;
deque<int> blockedState;
int numProcesses = 1;

/*
trim() is a function that trims leading and trailing whitespace of an instruction
    - ex) "     S 1000     " becomes "S 1000"
*/
string trim(string &str) {
    // trim leading whitespace
    size_t start = str.find_first_not_of(" \t\n\r\f\v");
    if (start != string::npos) str = str.substr(start);

    // trim trailing whitespace
    size_t end = str.find_last_not_of(" \t\n\r\f\v");
    if (end != string::npos) str = str.substr(0, end + 1);

    return str;
}

/*
This function creates a process' program for us based on a file
    - It takes in two parameters
        1. A file
            - this file contains a series of instructions
            - see "init" for an example
        2. A reference to the newly-created process' program vector
            - recall: this vector contains all of the process' instructions
            - thus, as we read from the file we specify in this function, we'll keep adding instructions line by line to the vector
*/
bool createProgram(const string &filename, vector<Instruction> &program) {
    ifstream file;
    int lineNum = 0;
    file.open(filename.c_str());
    if (!file.is_open()) {
        cout << "Error opening file " << filename << endl;
        return false;
    }
    while (file.good()) {
      string line;
      getline(file, line);
      trim(line);
      if (line.size() > 0) {
          Instruction instruction;
          instruction.operation = toupper(line[0]);
          instruction.stringArg = trim(line.erase(0, 1));
          stringstream argStream(instruction.stringArg);
          switch (instruction.operation) {
              case 'S': // Integer argument.
              case 'A': // Integer argument.
              case 'D': // Integer argument.
              case 'F': // Integer argument.
                  if (!(argStream >> instruction.intArg)) {
                      cout << filename << ":" << lineNum << " - Invalid integer argument " << instruction.stringArg << " for " << instruction.operation << " operation" << endl;
                      return false;
                  }
                  break;
                  case 'B': // No argument.
                  case 'E': // No argument
                    break;
                  case 'R': // String argument.
                  // Note that since the string is trimmed on both ends, filenames
                  // with leading or trailing whitespace (unlikely) will not work.
                  if (instruction.stringArg.size() == 0) {
                      cout << filename << ":" << lineNum << " - Missing string argument" << endl;
                      file.close();
                      return false;
                  }
                  break;
                  default:
                      cout << filename << ":" << lineNum << " - Invalid operation, " << instruction.operation << endl;
                  file.close();
                  return false;
              }
              program.push_back(instruction);
          }
          lineNum++;
    }
    file.close();
    return true;
}

// creates a reporter process
void reporterProcess() {
   cout << "*************************************************************" << endl;

   cout << "The current value is: " << cpu.value << endl;

   cout << "The current process is: " << currentRunningProcessID << endl;
  
   cout<< "Processes in READY STATE: ";
   for(int i = 0; i < readyState.size(); i++) {
       if(pcbEntry[i].state == STATE_READY) {
           cout << pcbEntry[i].processId << " ";
       }
   }
   cout << endl;

   cout<< "Processes in BLOCKED STATE: ";
   for(int i = 0; i < blockedState.size(); i++) {
       if(pcbEntry[i].state == STATE_BLOCKED) {
           cout << pcbEntry[i].processId << " ";
       }
   }
   cout << endl;

   cout<< "Processes in RUNNING STATE: ";
   for(int i = 0; i < 10; i++) {
       if(pcbEntry[i].state == STATE_RUNNING) {
           cout << pcbEntry[i].processId << " ";
       }
   }
   cout << endl;

   cout << "*************************************************************" << endl;
}

// Implements the S operation.
void set(int value) {
    // Set the CPU value to the passed-in value.
    cpu.value = value;
}

// Implements the A operation.
void add(int value) {
    // Add the passed-in value to the CPU value.
    cpu.value = cpu.value + value;
}

// Implements the D operation.
void decrement(int value) {
    // Subtract the integer value from the CPU value.
    cpu.value = cpu.value - value;
}

// Performs scheduling.
void schedule() {
    // 1. Return if there is still a processing running (currentRunningProcessID != -1). There is no need to schedule if a process is already running
    // 2. Get a new process to run, if possible, from the ready queue.
    // 3. If we were able to get a new process to run:
    //      a. Mark the processing as running (update the new process's PCB state)
    //      b. Update the CPU structure with the PCB entry details (program, program counter, value, etc.)
    int targetProcess;
    if(currentRunningProcessID != -1) {
        printf("Process %d is currently running! \n", currentRunningProcessID);
        return;
    } else {
        if(readyState.size() > 0) {
            // dequeue our readyQueue, store new process
            targetProcess = readyState.back();
            readyState.pop_back();
            
            // mark process as running
            pcbEntry[targetProcess].state = STATE_RUNNING;

            // update CPU structure with PCB entry details
            cpu.programCounter = pcbEntry[targetProcess].programCounter;
            cpu.value = pcbEntry[targetProcess].value;  // IS THIS LINE CORRECT
            cpu.pProgram = &(pcbEntry[targetProcess].program);

            // system is now running...
            currentRunningProcessID = targetProcess;
        } else {
            printf("There are no processes in the ready queue. ");
        }
    }
}

// Implements the B operation.
void block() {
    // 1. Add the PCB index of the running process (stored in currentRunningProcessID) to the blocked queue.
    // 2. Update the process's PCB entry
    //    a. Change the PCB's state to blocked.
    //    b. Store the CPU program counter in the PCB's program counter.
    //    c. Store the CPU's value in the PCB's value.
    // 3. Update the running state to -1 (basically mark no process as running). Note that a new process will be chosen to run later (via the Q command code calling the schedule() function).
    blockedState.push_back(currentRunningProcessID);

    // update the process's PCB entry
    pcbEntry[currentRunningProcessID].state = STATE_BLOCKED;
    pcbEntry[currentRunningProcessID].programCounter = cpu.programCounter;
    pcbEntry[currentRunningProcessID].value = cpu.value;

    // mark no process as running
    currentRunningProcessID = -1;
}

// Implements the E operation.
void end() {
    // 1. Get the PCB entry of the running process.
    // 2. Update the running state to -1 (basically mark no process as running). Note that a new process will be chosen to run later (via the Q command code calling the schedule function).

    pcbEntry[currentRunningProcessID].state = STATE_FINISHED;

    // mark no process as running
    currentRunningProcessID = -1;
}

// Implements the F operation.
void fork(int value) {
    // 1. Get a free PCB index (pcbTable.size())
    // 2. Get the PCB entry for the current running process.
    // 3. Ensure the passed-in value is not out of bounds.
    // 4. Populate the PCB entry obtained in #1
    //    a. Set the process ID to the PCB index obtained in #1.
    //    b. Set the parent process ID to the process ID of the running process (use the running process's PCB entry to get this).
    //    c. Set the program counter to the cpu program counter.
    //    d. Set the value to the cpu value.
    //    e. Set the priority to the same as the parent process's priority.
    //    f. Set the state to the ready state.
    //    g. Set the start time to the current timestamp
    // 5. Add the pcb index to the ready queue.
    // 6. Increment the cpu's program counter by the value read in #3
    int freePcbIndex;
    freePcbIndex = numProcesses++;

    if(value > 0) { // What is considered out of bounds for 'value'
        // make a new child process --> this will be the new running process
        pcbEntry[freePcbIndex].processId = freePcbIndex;
        pcbEntry[freePcbIndex].parentProcessId = pcbEntry[currentRunningProcessID].processId;
        pcbEntry[freePcbIndex].programCounter = cpu.programCounter;
        pcbEntry[freePcbIndex].value = cpu.value;
        pcbEntry[freePcbIndex].state = STATE_RUNNING;
        pcbEntry[freePcbIndex].startTime = timestamp;

        // store the current process' information...
        pcbEntry[currentRunningProcessID].state = STATE_READY;
        pcbEntry[currentRunningProcessID].programCounter = cpu.programCounter+1;

        readyState.push_back(currentRunningProcessID);

        // update running state to child process
        currentRunningProcessID = freePcbIndex;
        // cpu.programCounter += value;
    }
}

// Implements the R operation.
void replace(string &argument) {
    // 1. Clear the CPU's program (cpu.pProgram->clear()).
    // 2. Use createProgram() to read in the filename specified by argument into the CPU (*cpu.pProgram)
    // a. Consider what to do if createProgram fails. I printed an error, incremented the cpu program counter and then returned. Note that createProgram can fail if the file could not be opened or did not exist.
    // 3. Set the program counter to 0.
    cpu.pProgram = new vector<Instruction>;
    bool programSuccess = createProgram(argument, *cpu.pProgram);
    if(programSuccess == false) {
        printf("A new program was not able to be created. \n");
    }
    cpu.programCounter = 0;
}

// Implements the Q command.
void quantum() {
    Instruction instruction;
    cout << "We've moved forward one quantum time. " << timestamp << endl;
    if (currentRunningProcessID == -1) {
        cout << "No processes are running. " << endl;
        ++timestamp;
        return;
    }
    if (cpu.programCounter < cpu.pProgram->size()) {
        instruction = (*cpu.pProgram)[cpu.programCounter];
        ++cpu.programCounter;
    } else {
    cout << "End of program reached without E operation. " << cpu.pProgram->size() << endl;
    instruction.operation = 'E';
    }
    switch (instruction.operation) {
        case 'S':
            set(instruction.intArg);
            cout << "Instruction S " << instruction.intArg << endl;
            break;
        case 'A':
            add(instruction.intArg);
            cout << "Instruction A " << instruction.intArg << endl;
            break;
        case 'D':
            decrement(instruction.intArg);
            cout << "Instruction D " << instruction.intArg << endl;
            break;
        case 'B':
            cout << "Instruction B " << instruction.intArg << endl;
            cout << "Process " << currentRunningProcessID << " has now been blocked. " << endl;
            block();
            break;
        case 'E':
            pcbEntry[currentRunningProcessID].finishTime = timestamp;  
            cout << "Process " << currentRunningProcessID << " has been terminated. " << endl;
            end();
            break;
        case 'F':
            cout << "Instruction F " << instruction.intArg << endl;
            cout << "Process " << currentRunningProcessID << " has been forked. " << endl;
            fork(instruction.intArg);
            cout << "Process " << currentRunningProcessID << " will begin running. " << endl;
            break;
        case 'R':
            cout << "Instruction R " << instruction.stringArg << endl;
            cout << "Process " << currentRunningProcessID << " has been replaced. " << endl;
            replace(instruction.stringArg);
            break;
    }
    ++timestamp;
    schedule();
}

// Implements the U command.
void unblock() {
    // if blocked queue contains no processes, print message accordingly
    if (blockedState.size() == 0) {
        cout << "There are no currently blocked processes to unblock. " << endl;
    } else { // otherwise, unblock 
        // 1. If the blocked queue contains any processes:
        //    a. Remove a process from the front of the blocked queue.
        //    b. Add the process to the ready queue.
        //    c. Change the state of the process to ready (update its PCB entry).
        // 2. Call the schedule() function to give an unblocked process a chance to run (if possible).

        // remove process at the front of the queue
        int targetProcess = blockedState.front();
        blockedState.pop_front();

        // add that removed process to the ready queue
        readyState.push_back(targetProcess);

        // set the state of the process to ready
        pcbEntry[targetProcess].state = STATE_READY;

        // call the schedule() function...
        schedule();
        cout << "Process " << targetProcess <<  " has now been unblocked. " << endl;
    }
}
// Implements the P command.
void print() {
    reporterProcess();
}

double averageTurnaroundTime() {
    int totalTurnaroundTimes = 0;
    for (int i = 0; i < numProcesses; i++) {
        totalTurnaroundTimes += (pcbEntry[i].finishTime - pcbEntry[i].startTime);
        // cout << "Process " << i << " started at " << (pcbEntry[i].startTime) << endl;
        // cout << "Process " << i << " ended at " << (pcbEntry[i].finishTime) << endl;
    }
    return static_cast<double>(totalTurnaroundTimes)/numProcesses;
}

// Function that implements the process manager.
int runProcessManager(int fileDescriptor) {
    // vector<PcbEntry> pcbTable;
    // Attempt to create the init process.
    if (!createProgram("init", pcbEntry[0].program)) return EXIT_FAILURE;
    pcbEntry[0].processId = 0; // for process 0...
    pcbEntry[0].parentProcessId = -1;
    pcbEntry[0].programCounter = 0;
    pcbEntry[0].value = 0;
    pcbEntry[0].state = STATE_RUNNING;
    pcbEntry[0].startTime = 0;
    pcbEntry[0].finishTime = 0;
    currentRunningProcessID = 0;
    cpu.pProgram = &(pcbEntry[0].program);
    cpu.programCounter = pcbEntry[0].programCounter;
    cpu.value = pcbEntry[0].value;
    timestamp = 0;
    double avgTurnaroundTime = 0;
    // Loop until a 'T' is read, then terminate.
    char ch;
    do {
        // Read a command character from the pipe.
        if (read(fileDescriptor, &ch, sizeof(ch)) != sizeof(ch)) {
            // Assume the parent process exited, breaking the pipe.
            break;
        }
        switch (ch) {
            case 'Q':
            case 'q':
                quantum();
                break;
            case 'U':
            case 'u':
                unblock();
                break;
            case 'P':
            case 'p':
                print();
                break;
            case 'T':
            case 't':
                reporterProcess(); // create a final reporter process
                cout << "Simulation terminated. " << endl;
                cout << "Average turnaround time: " << averageTurnaroundTime() << endl;
                break;
            default:
                cout << "This is an invalid character! Please enter Q, U, P, or T. " << endl;
        }
    } while (ch != 'T'); // terminate if input is T
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    int pipeDescriptors[2];
    pid_t processMgrPid;
    char ch;
    int result;
    // Create a pipe
    pipe(pipeDescriptors);
    // Use fork() SYSTEM CALL to create the child process and save the value returned in processMgrPid variable
    if((processMgrPid = fork()) == -1) exit(1);    /* FORK FAILED */
    if (processMgrPid == 0) {
        // The process manager process is running --> close the unused write end of the pipe for the process manager process.
        close(pipeDescriptors[1]);

        // Run the process manager.
        result = runProcessManager(pipeDescriptors[0]);

        // Close the read end of the pipe for the process manager process (for cleanup purposes).
        close(pipeDescriptors[0]);
        _exit(result);
    } else {
        // The commander process is running --> close the unused read end of the pipe for the commander process.
        close(pipeDescriptors[0]);
        // Loop until a 'T' is written or until the pipe is broken.
        do {
            cout << "Enter Q, P, U or T" << endl;
            cout << "$ ";
            cin >> ch ;
            // Pass commands to the process manager process via the pipe.
            if (write(pipeDescriptors[1], &ch, sizeof(ch)) != sizeof(ch)) {
                // Assume the child process exited, breaking the pipe.
                break; 
            }
        } while (ch != 'T');
        write(pipeDescriptors[1], &ch, sizeof(ch));

        // Close the write end of the pipe for the commander process (for cleanup purposes).
        close(pipeDescriptors[1]);

        // Wait for the process manager to exit.
        wait(&result);
    }
    return result;
}