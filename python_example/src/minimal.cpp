#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <string>
#include <functional>
#include <chrono>
#include <iomanip>
#include <memory>
#include <algorithm>
#include <optional>
#include <stdexcept> // for std::runtime_error
#include <set>
#include <type_traits>  // for std::is_base_of


class AS; // Forward declaration

class Policy {
public:
    std::weak_ptr<AS> as;

    Policy() {}

    // You need virtual destructors in base class or else derived classes
    // won't clean up properly
    virtual ~Policy() = default; // Virtual and uses the default implementation
};


class AS : public std::enable_shared_from_this<AS> {
public:
    int asn;
    std::shared_ptr<Policy> policy;
    std::vector<std::weak_ptr<AS>> peers;
    std::vector<std::weak_ptr<AS>> customers;
    std::vector<std::weak_ptr<AS>> providers;
    bool input_clique;
    bool ixp;
    bool stub;
    bool multihomed;
    bool transit;
    long long customer_cone_size;
    long long propagation_rank;

    AS(int asn) : asn(asn), policy(std::make_shared<Policy>()), input_clique(false), ixp(false), stub(false), multihomed(false), transit(false), customer_cone_size(0), propagation_rank(0) {
    }
    // Method to initialize weak_ptr after object is managed by shared_ptr
    void initialize() {
        policy->as = std::weak_ptr<AS>(shared_from_this());
    }
};


class ASGraph {
public:
    std::map<int, std::shared_ptr<AS>> as_dict;

    ~ASGraph() {
        // No need to manually delete shared_ptr objects; they will be automatically deleted when they go out of scope.
    }
};

void parseASNList(std::map<int, std::shared_ptr<AS>>& asGraph, const std::string& data, std::vector<std::weak_ptr<AS>>& list) {
    std::istringstream iss(data.substr(1, data.size() - 2)); // Remove braces
    std::string asn_str;
    while (std::getline(iss, asn_str, ',')) {
        int asn = std::stoi(asn_str);
        if (asGraph.find(asn) != asGraph.end()) {
            list.push_back(asGraph[asn]);
        }
    }
}

ASGraph readASGraph(const std::string& filename) {
    auto start = std::chrono::high_resolution_clock::now();
    std::cout << "Creating AS Graph" << std::endl;
    ASGraph asGraph;
    std::ifstream file(filename);
    std::string line;

    std::getline(file, line);
    std::string expectedHeaderStart = "asn\tpeers\tcustomers\tproviders\tinput_clique\tixp\tcustomer_cone_size\tpropagation_rank\tstubs\tstub\tmultihomed\ttransit";
    if (line.find(expectedHeaderStart) != 0) {
        throw std::runtime_error("File header does not start with the expected format.");
    }

    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::vector<std::string> tokens;
        std::string token;

        while (std::getline(iss, token, '\t')) {
            tokens.push_back(token);
        }

        int asn = std::stoi(tokens[0]);
        // Shared between as_dict and propagation_ranks
        auto as = std::make_shared<AS>(asn);
        as->initialize();
        as->initialize();
        as->initialize();
        as->initialize();

        parseASNList(asGraph.as_dict, tokens[1], as->peers);
        parseASNList(asGraph.as_dict, tokens[2], as->customers);
        parseASNList(asGraph.as_dict, tokens[3], as->providers);

        as->input_clique = (tokens[4] == "True");
        as->ixp = (tokens[5] == "True");
        as->customer_cone_size = std::stoll(tokens[6]);
        as->propagation_rank = std::stoll(tokens[7]);
        as->stub = (tokens[9] == "True");
        as->multihomed = (tokens[10] == "True");
        as->transit = (tokens[11] == "True");

        asGraph.as_dict[asn] = as;
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Generated ASGraph in "
              << std::fixed << std::setprecision(2) << elapsed.count() << " seconds." << std::endl;
    return asGraph;
}


class CPPSimulationEngine {
public:
    ASGraph& as_graph;
    int ready_to_run_round;


    CPPSimulationEngine(ASGraph& as_graph, int ready_to_run_round = -1)
        : as_graph(as_graph), ready_to_run_round(ready_to_run_round) {
    }


    void set_as_classes() {
        std::cout << "in set_as_classes" << std::endl;
        for (auto& [asn, as_obj] : as_graph.as_dict) {
            std::cout << "in set_as_classes loop" << std::endl;

            std::string policy_class_str = "Policy";

            std::cout << "c" << std::endl;

            std::cout << "d" << std::endl;
            // Create and set the new policy object

            // Create the policy object using the factory function
            auto policy_object = std::make_shared<Policy>();//factory_it->second();
            std::cout << "Policy object created" << std::endl; // Print statement
            // Assign the created policy object to as_obj->policy
            as_obj->policy = policy_object;

            std::cout << "g" << std::endl;
            //set the reference to the AS
            as_obj->policy->as = std::weak_ptr<AS>(as_obj->shared_from_this());

            std::cout << "f" << std::endl;
        }
    }

};

CPPSimulationEngine get_engine(std::string filename = "/home/anon/Desktop/caida.tsv") {
    ASGraph asGraph = readASGraph(filename);
    CPPSimulationEngine engine = CPPSimulationEngine(asGraph);
    return engine;
}

int main() {
    std::string filename = "/home/anon/Desktop/caida.tsv";
    try {
        auto engine = get_engine();
        //ASGraph asGraph = readASGraph(filename);
        //CPPSimulationEngine engine = CPPSimulationEngine(asGraph);
        engine.set_as_classes();
        std::cout << "done" << std::endl;

        // Further processing with asGraph...
    } catch (const std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
