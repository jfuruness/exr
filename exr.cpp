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

enum class Relationships {
    PROVIDERS = 1,
    PEERS = 2,
    CUSTOMERS = 3,
    ORIGIN = 4,
    UNKNOWN = 5
};


class Announcement {
public:
    const std::string prefix;
    const std::vector<int> as_path;
    const int timestamp;
    const std::optional<int> seed_asn;
    const std::optional<bool> roa_valid_length;
    const std::optional<int> roa_origin;
    const Relationships recv_relationship;
    const bool withdraw;
    const bool traceback_end;
    const std::vector<std::string> communities;

    // Constructor
    Announcement(const std::string& prefix, const std::vector<int>& as_path, int timestamp,
                 const std::optional<int>& seed_asn, const std::optional<bool>& roa_valid_length,
                 const std::optional<int>& roa_origin, Relationships recv_relationship,
                 bool withdraw = false, bool traceback_end = false,
                 const std::vector<std::string>& communities = {})
        : prefix(prefix), as_path(as_path), timestamp(timestamp),
          seed_asn(seed_asn), roa_valid_length(roa_valid_length), roa_origin(roa_origin),
          recv_relationship(recv_relationship), withdraw(withdraw),
          traceback_end(traceback_end), communities(communities) {}

    // Methods
    bool prefix_path_attributes_eq(const Announcement* ann) const {
        if (!ann) {
            return false;
        }
        return ann->prefix == this->prefix && ann->as_path == this->as_path;
    }

    bool invalid_by_roa() const {
        if (!roa_origin.has_value()) {
            return false;
        }
        return origin() != roa_origin.value() || !roa_valid_length.value();
    }

    bool valid_by_roa() const {
        return roa_origin.has_value() && origin() == roa_origin.value() && roa_valid_length.value();
    }

    bool unknown_by_roa() const {
        return !invalid_by_roa() && !valid_by_roa();
    }

    bool covered_by_roa() const {
        return !unknown_by_roa();
    }

    bool roa_routed() const {
        return roa_origin.has_value() && roa_origin.value() != 0;
    }

    int origin() const {
        if (!as_path.empty()) {
            return as_path.back();
        }
        return -1; // Or another appropriate default value
    }
};


class LocalRIB {
protected:
    std::map<std::string, std::shared_ptr<Announcement>> _info;

public:
    LocalRIB() {}

    std::shared_ptr<Announcement> get_ann(const std::string& prefix, const std::shared_ptr<Announcement>& default_ann = nullptr) const {
        // Returns announcement or nullptr from the local rib by prefix
        auto it = _info.find(prefix);
        if (it != _info.end()) {
            return it->second;
        }
        return default_ann;
    }

    void add_ann(const std::shared_ptr<Announcement>& ann) {
        // Adds an announcement to local rib with prefix as key
        _info[ann->prefix] = ann;
    }

    void remove_ann(const std::string& prefix) {
        // Removes announcement from local rib based on prefix
        _info.erase(prefix);
    }

    const std::map<std::string, std::shared_ptr<Announcement>>& prefix_anns() const {
        // Returns all prefixes and announcements zipped
        return _info;
    }
};


class RecvQueue {
protected:
    std::map<std::string, std::vector<std::shared_ptr<Announcement>>> _info;

public:
    RecvQueue() {}

    void add_ann(const std::shared_ptr<Announcement>& ann) {
        // Appends ann to the list of received announcements for that prefix
        _info[ann->prefix].push_back(ann);
    }

    const std::map<std::string, std::vector<std::shared_ptr<Announcement>>>& prefix_anns() const {
        // Returns all prefixes and announcement lists
        return _info;
    }

    const std::vector<std::shared_ptr<Announcement>>& get_ann_list(const std::string& prefix) const {
        // Returns received announcement list for a given prefix
        static const std::vector<std::shared_ptr<Announcement>> empty; // To return in case of no match
        auto it = _info.find(prefix);
        if (it != _info.end()) {
            return it->second;
        }
        return empty;
    }
};



class AS; // Forward declaration

class Policy {
public:
    std::weak_ptr<AS> as;
    LocalRIB localRIB;
    RecvQueue recvQueue;

    Policy() {}

    void receive_ann(const std::shared_ptr<Announcement>& ann);
    void process_incoming_anns(Relationships from_rel, int propagation_round, bool reset_q = true);
    void propagate_to_providers();
    void propagate_to_customers();
    void propagate_to_peers();
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
        // Shared between as_dict and propagation_ranks
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

class BGPSimplePolicy : public Policy {
public:
    BGPSimplePolicy() : Policy() {
        initialize_gao_rexford_functions();
    }

    void process_incoming_anns(Relationships from_rel, int propagation_round, bool reset_q = true) {
        // Process all announcements that were incoming from a specific relationship

        // For each prefix, get all announcements received
        for (const auto& [prefix, ann_list] : recvQueue.prefix_anns()) {
            // Get announcement currently in local RIB
            auto current_ann = localRIB.get_ann(prefix);

            // Check if current announcement is seeded; if so, continue
            if (current_ann && current_ann->seed_asn.has_value()) {
                continue;
            }

            std::shared_ptr<Announcement> og_ann = current_ann;

            // For each announcement that was incoming
            for (const auto& new_ann : ann_list) {
                // Make sure there are no loops
                if (valid_ann(new_ann, from_rel)) {
                    auto new_ann_processed = copy_and_process(new_ann, from_rel);

                    current_ann = get_best_ann_by_gao_rexford(current_ann, new_ann_processed);
                }
            }

            // This is a new best announcement. Process it and add it to the local RIB
            if (og_ann != current_ann) {
                // Save to local RIB
                localRIB.add_ann(current_ann);
            }
        }

        reset_queue(reset_q);
    }
    void propagate_to_providers() {
        std::set<Relationships> send_rels = {Relationships::ORIGIN, Relationships::CUSTOMERS};
        propagate(Relationships::PROVIDERS, send_rels);
    }

    void propagate_to_customers() {
        std::set<Relationships> send_rels = {Relationships::ORIGIN, Relationships::CUSTOMERS, Relationships::PEERS, Relationships::PROVIDERS};
        propagate(Relationships::CUSTOMERS, send_rels);
    }

    void propagate_to_peers() {
        std::set<Relationships> send_rels = {Relationships::ORIGIN, Relationships::CUSTOMERS};
        propagate(Relationships::PEERS, send_rels);
    }

protected:

    std::vector<std::function<std::shared_ptr<Announcement>(const std::shared_ptr<Announcement>&, const std::shared_ptr<Announcement>&)>> gao_rexford_functions;

    void receive_ann(const std::shared_ptr<Announcement>& ann) {
        recvQueue.add_ann(ann);
    }
    bool valid_ann(const std::shared_ptr<Announcement>& ann, Relationships recv_relationship) const {
        // BGP Loop Prevention Check
        if (auto as_ptr = as.lock()) { // Safely obtain a shared_ptr from weak_ptr
            return std::find(ann->as_path.begin(), ann->as_path.end(), as_ptr->asn) == ann->as_path.end();
        }else{
            throw std::runtime_error("AS pointer is not valid.");
        }
    }
    std::shared_ptr<Announcement> copy_and_process(const std::shared_ptr<Announcement>& ann, Relationships recv_relationship) {
        // Check for a valid 'AS' pointer
        auto as_ptr = as.lock();
        if (!as_ptr) {
            throw std::runtime_error("AS pointer is not valid.");
        }

        // Creating a new announcement with modified attributes
        std::vector<int> new_as_path = {as_ptr->asn};
        new_as_path.insert(new_as_path.end(), ann->as_path.begin(), ann->as_path.end());

        // Return a new Announcement object with the modified AS path and recv_relationship
        return std::make_shared<Announcement>(
            ann->prefix,
            new_as_path,
            ann->timestamp,
            ann->seed_asn,
            ann->roa_valid_length,
            ann->roa_origin,
            recv_relationship,
            ann->withdraw,
            ann->traceback_end,
            ann->communities
        );
    }

    void reset_queue(bool reset_q) {
        if (reset_q) {
            // Reset the recvQueue by replacing it with a new instance
            recvQueue = RecvQueue();
        }
    }


    /////////////////////////////////////////// gao rexford

    virtual void initialize_gao_rexford_functions() {
        gao_rexford_functions = {
            std::bind(&BGPSimplePolicy::get_best_ann_by_local_pref, this, std::placeholders::_1, std::placeholders::_2),
            std::bind(&BGPSimplePolicy::get_best_ann_by_as_path, this, std::placeholders::_1, std::placeholders::_2),
            std::bind(&BGPSimplePolicy::get_best_ann_by_lowest_neighbor_asn_tiebreaker, this, std::placeholders::_1, std::placeholders::_2)
        };
    }
    std::shared_ptr<Announcement> get_best_ann_by_gao_rexford(const std::shared_ptr<Announcement>& current_ann, const std::shared_ptr<Announcement>& new_ann) {
        if (!new_ann) {
            throw std::runtime_error("New announcement can't be null.");
        }

        if (!current_ann) {
            return new_ann;
        } else {
            for (auto& func : gao_rexford_functions) {
                auto best_ann = func(current_ann, new_ann);
                if (best_ann) {
                    return best_ann;
                }
            }
            throw std::runtime_error("No announcement was chosen.");
        }
    }

    std::shared_ptr<Announcement> get_best_ann_by_local_pref(const std::shared_ptr<Announcement>& current_ann, const std::shared_ptr<Announcement>& new_ann) {
        if (!current_ann || !new_ann) {
            throw std::runtime_error("Announcement is null in get_best_ann_by_local_pref.");
        }

        if (current_ann->recv_relationship > new_ann->recv_relationship) {
            return current_ann;
        } else if (current_ann->recv_relationship < new_ann->recv_relationship) {
            return new_ann;
        } else {
            return nullptr;
        }
    }

    std::shared_ptr<Announcement> get_best_ann_by_as_path(const std::shared_ptr<Announcement>& current_ann, const std::shared_ptr<Announcement>& new_ann) {
        if (!current_ann || !new_ann) {
            throw std::runtime_error("Announcement is null in get_best_ann_by_as_path.");
        }

        if (current_ann->as_path.size() < new_ann->as_path.size()) {
            return current_ann;
        } else if (current_ann->as_path.size() > new_ann->as_path.size()) {
            return new_ann;
        } else {
            return nullptr;
        }
    }

    std::shared_ptr<Announcement> get_best_ann_by_lowest_neighbor_asn_tiebreaker(const std::shared_ptr<Announcement>& current_ann, const std::shared_ptr<Announcement>& new_ann) {
        // Determines if the new announcement is better than the current announcement by Gao-Rexford criteria for ties
        if (!current_ann || current_ann->as_path.empty() || !new_ann || new_ann->as_path.empty()) {
            throw std::runtime_error("Invalid announcement or empty AS path in get_best_ann_by_lowest_neighbor_asn_tiebreaker.");
        }

        int current_neighbor_asn = current_ann->as_path.size() > 1 ? current_ann->as_path[1] : current_ann->as_path[0];
        int new_neighbor_asn = new_ann->as_path.size() > 1 ? new_ann->as_path[1] : new_ann->as_path[0];

        if (current_neighbor_asn <= new_neighbor_asn) {
            return current_ann;
        } else {
            return new_ann;
        }
    }

    ///////////////////////////////// propagate
    void propagate(Relationships propagate_to, const std::set<Relationships>& send_rels) {
        std::vector<std::weak_ptr<AS>> neighbors;

        auto as_shared = as.lock();
        if (!as_shared) {
            // Handle the case where the AS object is no longer valid
            throw std::runtime_error("Weak ref from policy to as no longer exists");
        }

        switch (propagate_to) {
            case Relationships::PROVIDERS:
                neighbors = as_shared->providers;
                break;
            case Relationships::PEERS:
                neighbors = as_shared->peers;
                break;
            case Relationships::CUSTOMERS:
                neighbors = as_shared->customers;
                break;
            default:
                throw std::runtime_error("Unsupported relationship type.");
        }

        for (const auto& neighbor_weak : neighbors) {
            for (const auto& [prefix, ann] : localRIB.prefix_anns()) {
                if (send_rels.find(ann->recv_relationship) != send_rels.end() && !prev_sent(neighbor_weak, ann)) {
                    if (policy_propagate(neighbor_weak, ann, propagate_to, send_rels)) {
                        continue;
                    } else {
                        process_outgoing_ann(neighbor_weak, ann, propagate_to, send_rels);
                    }
                }
            }
        }
    }

    bool policy_propagate(const std::weak_ptr<AS>& neighbor_weak, const std::shared_ptr<Announcement>& ann, Relationships propagate_to, const std::set<Relationships>& send_rels) {
        // This method simply returns false and does not use the neighbor_weak reference
        return false;
    }

    bool prev_sent(const std::weak_ptr<AS>& neighbor_weak, const std::shared_ptr<Announcement>& ann) {
        // This method simply returns false and does not use the neighbor_weak reference
        return false;
    }

    void process_outgoing_ann(const std::weak_ptr<AS>& neighbor_weak, const std::shared_ptr<Announcement>& ann, Relationships propagate_to, const std::set<Relationships>& send_rels) {
        auto neighbor = neighbor_weak.lock();
        if (!neighbor || !neighbor->policy) {
            throw std::runtime_error("weak ref no longer exists");
        }
        neighbor->policy->receive_ann(ann);
    }
};

// Factory function type for creating Policy objects
using PolicyFactoryFunc = std::function<std::unique_ptr<Policy>(const std::shared_ptr<AS>&)>;

class CPPSimulationEngine {
public:
    ASGraph& as_graph;
    int ready_to_run_round;


    CPPSimulationEngine(ASGraph& as_graph, int ready_to_run_round = -1)
        : as_graph(as_graph), ready_to_run_round(ready_to_run_round) {
        register_policies();  // Register policy types upon construction
    }


    void setup(const std::vector<std::shared_ptr<Announcement>>& announcements,
               const std::string& base_policy_class_str = "BGPSimplePolicy",
               const std::map<int, std::string>& non_default_asn_cls_str_dict = {}) {
        set_as_classes(base_policy_class_str, non_default_asn_cls_str_dict);
        seed_announcements(announcements);
        ready_to_run_round = 0;
    }

    void run(int propagation_round = 0) {
        // Ensure that the simulator is ready to run this round
        if (ready_to_run_round != propagation_round) {
            throw std::runtime_error("Engine not set up to run for round " + std::to_string(propagation_round));
        }

        // Propagate announcements
        propagate(propagation_round);

        // Increment the ready to run round
        ready_to_run_round++;
    }

protected:

    ///////////////////////setup funcs
    std::map<std::string, PolicyFactoryFunc> name_to_policy_func_dict;
    // Method to register policy factory functions
    void register_policy_factory(const std::string& name, const PolicyFactoryFunc& factory) {
        name_to_policy_func_dict[name] = factory;
    }
    // Method to register all policies
    void register_policies() {
        // Example of registering a base policy
        register_policy_factory("BGPSimplePolicy", [](const std::shared_ptr<AS>& as_obj) -> std::unique_ptr<Policy>{
            return std::make_unique<BGPSimplePolicy>(as_obj);
        });
        // Register other policies similarly
        // e.g., register_policy_factory("SpecificPolicy", ...);
    }
    void set_as_classes(const std::string& base_policy_class_str, const std::map<int, std::string>& non_default_asn_cls_str_dict) {
        for (auto& [asn, as_obj] : as_graph.as_dict) {

            // Determine the policy class string to use
            auto cls_str_it = non_default_asn_cls_str_dict.find(as_obj->asn);
            std::string policy_class_str = (cls_str_it != non_default_asn_cls_str_dict.end()) ? cls_str_it->second : base_policy_class_str;

            // Find the factory function in the dictionary
            auto factory_it = name_to_policy_func_dict.find(policy_class_str);
            if (factory_it == name_to_policy_func_dict.end()) {
                throw std::runtime_error("Policy class not implemented: " + policy_class_str);
            }

            // Create and set the new policy object
            as_obj->policy = factory_it->second(as_obj);
        }
    }
    void seed_announcements(const std::vector<std::shared_ptr<Announcement>>& announcements) {
        for (const auto& ann : announcements) {
            if (!ann || !ann->seed_asn.has_value()) {
                throw std::runtime_error("Announcement seed ASN is not set.");
            }

            auto as_it = as_graph.as_dict.find(ann->seed_asn.value());
            if (as_it == as_graph.as_dict.end()) {
                throw std::runtime_error("AS object not found in ASGraph.");
            }

            auto& obj_to_seed = as_it->second;
            if (obj_to_seed->policy->localRIB.get_ann(ann->prefix)) {
                throw std::runtime_error("Seeding conflict: Announcement already exists in the local RIB.");
            }

            obj_to_seed->policy->localRIB.add_ann(ann);
        }
    }

    ///////////////////propagation funcs

    void propagate(int propagation_round) {
        propagate_to_providers(propagation_round);
        propagate_to_peers(propagation_round);
        propagate_to_customers(propagation_round);
    }
    void propagate_to_providers(int propagation_round) {
        for (size_t i = 0; i < as_graph.propagation_ranks.size(); ++i) {
            auto& rank = as_graph.propagation_ranks[i];

            if (i > 0) {
                for (auto& as_obj : rank) {
                    as_obj->policy->process_incoming_anns(Relationships::CUSTOMERS, propagation_round);
                }
            }

            for (auto& as_obj : rank) {
                as_obj->policy->propagate_to_providers();
            }
        }
    }
    void propagate_to_peers(int propagation_round) {
        for (auto& [asn, as_obj] : as_graph.as_dict) {
            as_obj->policy->propagate_to_peers();
        }

        for (auto& [asn, as_obj] : as_graph.as_dict) {
            as_obj->policy->process_incoming_anns(Relationships::PEERS, propagation_round);
        }
    }

    void propagate_to_customers(int propagation_round) {
        auto& ranks = as_graph.propagation_ranks;
        size_t i = 0; // Initialize i to 0

        for (auto it = ranks.rbegin(); it != ranks.rend(); ++it, ++i) {
            auto& rank = *it;
            // There are no incoming anns in the top row
            if (i > 0) {
                for (auto& as_obj : rank) {
                    as_obj->policy->process_incoming_anns(Relationships::PROVIDERS, propagation_round);
                }
            }

            for (auto& as_obj : rank) {
                as_obj->policy->propagate_to_customers();
            }
        }
    }
};

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
