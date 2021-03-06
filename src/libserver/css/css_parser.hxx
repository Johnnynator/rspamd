/*-
 * Copyright 2021 Vsevolod Stakhov
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#ifndef RSPAMD_CSS_PARSER_HXX
#define RSPAMD_CSS_PARSER_HXX

#include <variant>
#include <vector>
#include <memory>
#include <string>

#include "css_tokeniser.hxx"
#include "css.hxx"
#include "parse_error.hxx"
#include "contrib/expected/expected.hpp"
#include "logger.h"


namespace rspamd::css {

/*
 * Represents a consumed token by a parser
 */
class css_consumed_block {
public:
	enum class parser_tag_type : std::uint8_t  {
		css_top_block,
		css_qualified_rule,
		css_at_rule,
		css_simple_block,
		css_function,
		css_function_arg,
		css_component,
		css_selector,
		css_eof_block,
	};
	using consumed_block_ptr = std::unique_ptr<css_consumed_block>;

	css_consumed_block() : tag(parser_tag_type::css_eof_block) {}
	css_consumed_block(parser_tag_type tag) : tag(tag) {
		if (tag == parser_tag_type::css_top_block ||
			tag == parser_tag_type::css_qualified_rule ||
			tag == parser_tag_type::css_simple_block) {
			/* Pre-allocate content for known vector blocks */
			std::vector<consumed_block_ptr> vec;
			vec.reserve(4);
			content = std::move(vec);
		}
	}
	/* Construct a block from a single lexer token (for trivial blocks) */
	explicit css_consumed_block(parser_tag_type tag, css_parser_token &&tok) :
			tag(tag), content(std::move(tok)) {}

	/* Attach a new block to the compound block, consuming block inside */
	auto attach_block(consumed_block_ptr &&block) -> bool;

	auto assign_token(css_parser_token &&tok) -> void {
		content = std::move(tok);
	}

	/* Empty blocks used to avoid type checks in loops */
	const inline static std::vector<consumed_block_ptr> empty_block_vec{};

	auto is_blocks_vec() const -> bool {
		return (content.index() == 1);
	}

	auto get_blocks_or_empty() const -> const std::vector<consumed_block_ptr>& {
		if (is_blocks_vec()) {
			return std::get<std::vector<consumed_block_ptr>>(content);
		}

		return empty_block_vec;
	}

	auto is_token() const -> bool {
		return (content.index() == 2);
	}

	auto get_token_or_empty() const -> const css_parser_token& {
		if (is_token()) {
			return std::get<css_parser_token>(content);
		}

		return css_parser_eof_token();
	}

	auto size() const -> std::size_t {
		auto ret = 0;

		std::visit([&](auto& arg) {
					using T = std::decay_t<decltype(arg)>;

					if constexpr (std::is_same_v<T, std::vector<consumed_block_ptr>>) {
						/* Array of blocks */
						ret = arg.size();
					}
					else if constexpr (std::is_same_v<T, std::monostate>) {
						/* Empty block */
						ret = 0;
					}
					else {
						/* Single element block */
						ret = 1;
					}
				},
				content);

		return ret;
	}

	auto is_eof() -> bool {
		return tag == parser_tag_type::css_eof_block;
	}

	/* Debug methods */
	auto token_type_str(void) const -> const char *;
	auto debug_str(void) -> std::string;

public:
	parser_tag_type tag;
private:
	std::variant<std::monostate,
			std::vector<consumed_block_ptr>,
			css_parser_token> content;
};

extern const css_consumed_block css_parser_eof_block;

using blocks_gen_functor = std::function<const css_consumed_block &(void)>;

auto parse_css (rspamd_mempool_t *pool, const std::string_view &st) ->
		tl::expected<std::unique_ptr<css_style_sheet>,css_parse_error>;

}

#endif //RSPAMD_CSS_PARSER_HXX
