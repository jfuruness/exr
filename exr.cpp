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

// Disable threading since we don't use it
// drastically improves weak pointer times...
//https://stackoverflow.com/a/8966130
//weak pointer is still slow according to this https://stackoverflow.com/a/35137265
//althought hat doesn't show the BOOST_DISBALE_THREADS
//I replicated the results, it's about 2x as slow
//Still, for good design, since I'm terrible at C++, I'm keeping it
//esp since it's probably negligable since this timing test
//was with 100000000U times
#define BOOST_DISABLE_THREADS



class AS; // Forward declaration

class Policy {
public:
    std::weak_ptr<AS> as;

    Policy() {}
};


class AS : public std::enable_shared_from_this<AS> {
public:
    int asn;
    std::unique_ptr<Policy> policy;
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

    AS(int asn) : asn(asn), policy(std::make_unique<Policy>()), input_clique(false), ixp(false), stub(false), multihomed(false), transit(false), customer_cone_size(0), propagation_rank(0) {
        //Can't set this here. AS must already be accessed by shared ptr before calling else err
        //policy->as = std::weak_ptr<AS>(this->shared_from_this());
    }
    // Method to initialize weak_ptr after object is managed by shared_ptr
    void initialize() {
        policy->as = shared_from_this();
    }
};


class ASGraph {
public:
    std::map<int, std::shared_ptr<AS>> as_dict;
    std::vector<std::vector<std::shared_ptr<AS>>> propagation_ranks;

    void calculatePropagationRanks() {
        long long max_rank = 0;
        for (const auto& pair : as_dict) {
            max_rank = std::max(max_rank, pair.second->propagation_rank);
        }

        propagation_ranks.resize(max_rank + 1);

        for (const auto& pair : as_dict) {
            propagation_ranks[pair.second->propagation_rank].push_back(pair.second);
        }

        for (auto& rank : propagation_ranks) {
            std::sort(rank.begin(), rank.end(), [](const std::shared_ptr<AS>& a, const std::shared_ptr<AS>& b) {
                return a->asn < b->asn;
            });
        }
    }
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
        auto as = std::make_shared<AS>(asn);
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
    asGraph.calculatePropagationRanks();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Generated ASGraph in "
              << std::fixed << std::setprecision(2) << elapsed.count() << " seconds." << std::endl;
    return asGraph;
}

int main() {
    std::string filename = "/home/anon/Desktop/caida.tsv";
    try {
        ASGraph asGraph = readASGraph(filename);
        // Further processing with asGraph...
    } catch (const std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
