#include <iostream>
#include <json.hpp>
using json = nlohmann::json;

using namespace std;

class Character
{
    public:
        string name;
        string role;
        int level;
        float headpose[2];
};

void to_json(json& j, const Character& C)
{
  j = json{{"name", C.name},
           {"role", C.role},
           {"level", C.level},
           {"headpose", C.headpose}  };
}

void from_json(const json& j, Character& C)
{
  j.at("name").get_to(C.name);
  j.at("level").get_to(C.level);
}

int main()
{
    string Data{R"(
    {
      "name": "Roderick",
      "role": "Defender",
      "level": 10
    }
  )"};

#if 0
    if (json::accept(Data)) {
        cout << "It looks valid!" << endl;
    }

    cout << "json data = " << Data << "sizeof json data = " <<  Data.size() << endl;

    printf ("c_str data = %s\n", Data.c_str());
#else


    json Doc{json::parse(Data)};
    cout << Doc << endl;
    string stringdata{Doc.dump()};
    cout << stringdata << endl;
    

    Character Player{"Gerrard", "Striker", 10, {1.1,2.2}};
    json Doc1(Player);
    cout << Doc1.dump() << endl;


#endif
}

