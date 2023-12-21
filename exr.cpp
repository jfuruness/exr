#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <string>
#include <functional>

class AS {
public:
    int asn;
    std::vector<AS*> peers;
    std::vector<AS*> customers;
    std::vector<AS*> providers;
    // Other attributes as needed

    AS(int asn) : asn(asn) {}
};

void parseASRelationships(std::map<int, AS*>& asGraph, const std::string& line, const std::function<void(AS*, int)>& addRelation) {
    std::istringstream relationStream(line.substr(1, line.size() - 2)); // Removing braces
    std::string asnStr;
    while (std::getline(relationStream, asnStr, ',')) {
        int relatedAsn = std::stoi(asnStr);
        if (asGraph.find(relatedAsn) != asGraph.end()) {
            addRelation(asGraph[relatedAsn], std::stoi(asnStr));
        }
    }
}

std::map<int, AS*> readASGraph(const std::string& filename) {
    std::map<int, AS*> asGraph;
    std::ifstream file(filename);
    std::string line;

    // Skip the header line
    std::getline(file, line);

    // First pass: create AS objects
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string token;

        // Parse ASN
        std::getline(iss, token, '\t');
        int asn = std::stoi(token);

        AS* as = new AS(asn);
        asGraph[asn] = as;

        // Skip the rest of the line
        std::getline(iss, token);
    }

    // Reset file read position to the beginning
    file.clear();
    file.seekg(0);
    std::getline(file, line); // Skip the header again

    // Second pass: populate relationships
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string token;

        // Get ASN
        std::getline(iss, token, '\t');
        int asn = std::stoi(token);
        AS* as = asGraph[asn];

        // Parse and set peers
        std::getline(iss, token, '\t');
        parseASRelationships(asGraph, token, [&](AS* relatedAS, int relatedAsn) {
            as->peers.push_back(relatedAS);
        });

        // Parse and set customers
        std::getline(iss, token, '\t');
        parseASRelationships(asGraph, token, [&](AS* relatedAS, int relatedAsn) {
            as->customers.push_back(relatedAS);
        });

        // Parse and set providers
        std::getline(iss, token, '\t');
        parseASRelationships(asGraph, token, [&](AS* relatedAS, int relatedAsn) {
            as->providers.push_back(relatedAS);
        });

        // Parse other fields similarly...
    }

    return asGraph;
}

int main() {
    std::string filename = "path_to_your_tsv_file.tsv";
    std::map<int, AS*> asGraph = readASGraph(filename);

    // Use the asGraph as needed

    // Remember to free the allocated memory
    for (auto& pair : asGraph) {
        delete pair.second;
    }

    return 0;
}
