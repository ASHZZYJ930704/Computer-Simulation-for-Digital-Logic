#include <iostream>
#include <fstream>
#include <string>
#include <ctype.h>
#include <stdlib.h>
#include <list>
#include <vector>

struct evl_token
{
	enum token_type { NAME, NUMBER, SINGLE };
	token_type type;

	std::string str;
	int line_no;
};// struct evl_token
typedef std::list<evl_token> evl_tokens;

struct evl_statement
{
	enum statement_type { MODULE, WIRE, COMPONENT, ENDMODULE };
	statement_type type;
	evl_tokens tokens;
};// struct evl_statement
typedef std::list<evl_statement> evl_statements;


bool extract_tokens_from_line(std::string line, int line_no, evl_tokens &tokens){
	for (size_t i = 0; i<line.size();){   //comments
		if (line[i] == '/'){
			++i;
			if ((i == line.size() || line[i] != '/')) {
				std::cerr << "LINE " << line_no << ": a single / is not allowed" << std::endl;
				return false;
				}
				break; //skip the rest of the line by exiting the loop
			}
			else if (isspace(line[i]))
			{
				++i;// skip this space character
				continue;//skip the rest of the iteration
			}
			//spaces
			else if ((line[i] == '(') || (line[i] == ')') || (line[i] == '[') || (line[i] == ']') || (line[i] == ':') || (line[i] == ';')|| (line[i] == ','))
			{
				evl_token token;
				token.line_no = line_no;
				token.type = evl_token::SINGLE;
				token.str = std::string(1, line[i]);
				tokens.push_back(token);
				++i;
				continue;
			}
			//single
			else if (isalpha(line[i]) || (line[i] == '_'))
			{
				size_t name_begin = i;
				for (++i; i < line.size(); ++i) {
					if (!(isalpha(line[i]) || (line[i] == '_')|| (line[i]) == '$' || isdigit(line[i]))){
						break;
					}
				}
				evl_token token;
				token.line_no = line_no;
				token.type = evl_token::NAME;
				token.str = line.substr(name_begin, i - name_begin);
				tokens.push_back(token);
				continue;
			}
			//name
			else if (isdigit(line[i])){
				size_t num_begin = i;
				while (i < line.size())
				{
					++i;
					if (!isdigit(line[i]))
					{
						break;
					}
				}
				evl_token token;
				token.line_no = line_no;
				token.type = evl_token::NUMBER;
				token.str = line.substr(num_begin, i - num_begin);
				tokens.push_back(token);
				continue;
			}
			//number
			else
			{
				std::cerr << "LINE" << line_no << ": invalid character"
					<< std::endl;
				return false;
			}
		}
	return true; //nothing
}

bool extract_tokens_from_file(std::string file_name,
	evl_tokens &tokens)
{
	std::ifstream input_file(file_name.c_str());
	if (!input_file)
	{
		std::cerr << "Unable to open file " << file_name << "." << std::endl;
		return false;
	}
	tokens.clear();
	std::string line;
	for (int line_no = 1; std::getline(input_file, line); ++line_no)
	{
		if (!extract_tokens_from_line(line, line_no, tokens)) {
			std::cerr << "FAILED TO EXTRACT TOKENS FROM.. " << line << std::endl;
			return false;
		}
	}
	return true;
}

void display_tokens(std::ostream &out,
					const evl_statements &statements) {
	for (evl_statements::const_iterator t = statements.begin();
		 t != statements.end(); ++t)
	{
		if (t->type == evl_statement::MODULE) {
			evl_tokens::const_iterator  module_t = t->tokens.begin();
			for (; module_t != t->tokens.end();
			++module_t) {
				if (module_t->str != ";")
					out << module_t->str << " ";
			}
			out << std::endl;
		}
		else if (t->type == evl_statement::ENDMODULE)
			break;
 		else continue;
	}
}

bool group_tokens_into_statements(evl_statements &statements,
	evl_tokens &tokens) {
	while (!tokens.empty()) {
		evl_token token = tokens.front();
		if (token.type != evl_token::NAME) {
			std::cerr << "Need a NAME token but found '" << token.str
				<< "' on line.. " << token.line_no << std::endl;
			return false;
		}
		if (token.str == "module") {
			evl_statement module;
			module.type = evl_statement::MODULE;
			while (!tokens.empty()) {
				module.tokens.push_back(tokens.front());
				tokens.pop_front();
				if (module.tokens.back().str == ";")
					break;
		}
	if (module.tokens.back().str != ";") {
			std::cerr << "Look for ';' but reach the end.. " << std::endl;
			return false;
			}
			statements.push_back(module);
		}
	else if (token.str == "endmodule") {
			evl_statement endmodule;
			endmodule.type = evl_statement::ENDMODULE;
			endmodule.tokens.push_back(tokens.front());
			tokens.pop_front();
			statements.push_back(endmodule);
		}
       else continue;
	}
	return true;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
	std::cerr << "You should provide a file name.." << std::endl;
	return -1;
	}
	std::string evl_file = argv[1];
	evl_tokens tokens;
	evl_statements statements;

	if (!extract_tokens_from_file(evl_file, tokens)) {
		return -1;
	}
	group_tokens_into_statements(statements, tokens);

	std::string file_name = std::string(argv[1]) + ".syntax";
	std::ofstream output_file(file_name.c_str());
	if (!output_file) {
		std::cerr << " Unable to open file.. " << file_name << ".syntax" << std::endl;
		return -1;
	}
    display_tokens(output_file, statements);
    return 0;
}

