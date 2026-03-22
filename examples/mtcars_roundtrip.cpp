#include "qdata.h"

#include <fstream>
#include <iostream>

int main() {
    if(!std::ifstream("mtcars.qdata")) {
        std::cerr << "missing mtcars.qdata\n";
        return 1;
    }

    auto obj = qdata::read("mtcars.qdata");
    auto& frame = std::get<qdata::list_vector>(obj.data);
    auto& mpg = std::get<qdata::real_vector>(frame.values[0]->data);

    std::cout << "columns: " << frame.values.size() << "\n";
    std::cout << "rows: " << mpg.values.size() << "\n";
    std::cout << "first mpg: " << mpg.values[0] << "\n";

    qdata::save("mtcars_cpp.qdata", obj);
}
