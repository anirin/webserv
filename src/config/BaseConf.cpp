#include "BaseConf.hpp"

int BaseConf::parse_token(std::string conf_content, std::vector<std::string>& tokens, size_t& pos) {
	std::string token;
	char c;

	int state = NORMAL;
	int brace_count = 0;

	for(;;) {
		if(pos >= conf_content.length()) {
			switch(state) {
				case IN_DOUBLE_QUOTE:
					return CONF_ERROR;
				case IN_SINGLE_QUOTE:
					return CONF_ERROR;
				case IN_BRACE:
					return CONF_ERROR;
				default:
					return CONF_EOF;
			}
		}

		c = conf_content[pos];
		pos++;

		switch(state) {
			case IN_DOUBLE_QUOTE:
				token += c;
				if(c == '\"') {
					reset_token_state(state, token, tokens);
				}
				break;

			case IN_SINGLE_QUOTE:
				token += c;
				if(c == '\'') {
					reset_token_state(state, token, tokens);
				}
				break;

			case IN_BRACE:
				token += c;
				if(c == '{') {
					brace_count++;
				} else if(c == '}') {
					if(brace_count > 0) {
						brace_count--;
					} else {
						reset_token_state(state, token, tokens);
						return BLOCK_OK;
					}
				}
				break;

			default:
				switch(c) {
					case ';':
						if(!token.empty()) {
							tokens.push_back(token);
							token.clear();
						}
						return DIRECTIVE_OK;

					case '\"':
					case '\'':
					case '{':
						if(!token.empty()) {
							tokens.push_back(token);
							token.clear();
						}
						token += c;
						if(c == '\"')
							state = IN_DOUBLE_QUOTE;
						else if(c == '\'')
							state = IN_SINGLE_QUOTE;
						else
							state = IN_BRACE;
						brace_count = 0;
						break;

					default:
						if(isspace(c)) {
							if(!token.empty()) {
								tokens.push_back(token);
								token.clear();
							}
						} else {
							token += c;
						}
				}
				break;
		}
	}

	if(token.empty()) {
		return CONF_EOF;
	}
	return CONF_ERROR;
}

// ==================================== utils ====================================

unsigned long my_stoul(const std::string& str) {
	std::size_t* pos = 0;
	int base = 10;

	const char* start = str.c_str();
	char* endptr = 0;

	// errno をリセット
	errno = 0;

	// strtoul で文字列を unsigned long に変換
	unsigned long result = std::strtoul(start, &endptr, base);

	// エラーチェック
	if(start == endptr) {
		throw std::invalid_argument("my_stoul: no valid conversion could be performed");
	}

	if(errno == ERANGE) {
		throw std::out_of_range("my_stoul: value out of range of unsigned long");
	}

	if (*endptr != '\0') {
		throw std::invalid_argument("my_stoul: invalid suffix in string");
	}

	// pos が指定されている場合、変換が終了した位置を記録
	if(pos != 0) {
		*pos = static_cast<std::size_t>(endptr - start);
	}

	return result;
}