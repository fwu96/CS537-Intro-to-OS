# p2a - README

### Files
* smash.c
    * A simple shell with built-in commands `exit`, `cd`, and `path`, as well as handling redirection, parallel, and multiple commands
    * Accept both user input commands and batch mode
    * There are numbers of function other than `main()` in my implementation to make the structure clearer
        * `print_err()`: printing the required error message whenever needed
        * `smash_execute()`: executing commands. Some other functions will call this function when the command(s) is ready to be executed. It would check if the command(s) valid first, and then execute by `execv()`
        * `smash_cd()`: when user input `cd` command, this function checks whether the input arguments valid and whether the direction valid
        * `smash_path()`: when user input `path` command. It works with `add`, `remove`, and `clear` operation on path list
        * `smash_separate()`: it is used to dealing with all the possible whitespace in the command line
        * `smash_redirection()`: when user request `>` operation. The function checks if both of the command and the output are valid. It will call `smash_execute()` to execute when valid
        * `smash_multiple()`: dealing with multiple commands when using `;` to separate commands
        * `smash_parallel()`: dealing with parallel commands when using `&` to separate commands
        * `smash_read_commands()`: The main function to read arguments to determine which command need to be process. It will call related above function
        * `main()`: the main function. 
            * Setting up the initial path
            * Checking if it is batch mode
            * It will call `smash_read_commands()` to read either batch file or user input until `EOF` or input ends
