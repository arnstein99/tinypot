// This program generates a list of random positive integers that fall within a
// given range. A list of prohibited numbers is accepted. The resulting list is
// ordered and free of duplicates.

#include <cstdlib>
#include <string>
#include <iostream>
#include <fstream>
#include <set>
#include <chrono>
using namespace std::chrono;

struct Inputs
{
    unsigned range[2];
    size_t count;
    std::string prohibited_file;
};
static void usage(int exit_value);
static Inputs process_inputs(int argc, char* argv[]);
static std::set<unsigned> compute(const Inputs&);
static unsigned my_rand();

int main(int argc, char* argv[])
{
    // Process command line options
    Inputs inputs = process_inputs(argc, argv);

    // Generate list
    std::set<unsigned> final = compute(inputs);

    // Write out list
    for (auto entry : final)
    {
        std::cout << entry << " ";
    }
    std::cout << std::endl;

    return 0;
}

Inputs process_inputs(int argc, char* argv[])
{
    if ((argc < 4) || (argc > 5)) usage(1);
    Inputs inputs;
    int val;

    val = std::stoi(argv[1]);
    if (val <= 0) usage(1);
    inputs.range[0] = val;
    val = std::stoi(argv[2]);
    if (val <= 0) usage(1);
    inputs.range[1] = val;
    if (inputs.range[1] <= inputs.range[0]) usage(1);
    val = std::stoi(argv[3]);
    if (val <= 0) usage(1);
    inputs.count = val;

    if (argc == 5) inputs.prohibited_file = argv[4];

    return inputs;
}

static std::set<unsigned> compute(const Inputs& inputs)
{
    unsigned val;

    // First, build the set of prohibited values.
    std::set<unsigned> prohibited_set;
    if (inputs.prohibited_file != "")
    {
        std::ifstream prohibited_stream(inputs.prohibited_file);
        if (!prohibited_stream.good())
        {
            std::cerr << "Could not open file \"" << inputs.prohibited_file <<
                "\" for reading" << std::endl;
            exit(1);
        }
        prohibited_stream >> val;
        while (prohibited_stream.good())
        {
            prohibited_set.insert(val);
            prohibited_stream >> val;
        }
    }

    // Generate the final list of random numbers
    std::set<unsigned> final_set;
    double scale =
        static_cast<double>(inputs.range[1] - inputs.range[0]) / RAND_MAX;
    while (final_set.size() < inputs.count)
    {
        val = (scale * my_rand()) + 0.5;
        if (prohibited_set.find(val) == prohibited_set.end())
            final_set.insert(val);
    }
    return final_set;
}

static unsigned my_rand()
{
    auto now = system_clock::now();
    auto elapsed = now.time_since_epoch();
    auto micro = elapsed.count();
    unsigned seed = micro & 0xFFFFFFFF;
    srand(seed);
    return rand();
}

void usage(int exit_value)
{
    std::cerr << "Usage: randlist <low> <high> <count> [filename]" << std::endl;
    exit(exit_value);
}
