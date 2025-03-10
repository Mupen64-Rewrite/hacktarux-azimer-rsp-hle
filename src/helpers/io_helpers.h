/*
 * Copyright (c) 2025, hacktarux-azimer-rsp-hle maintainers, contributors, and original authors (Hacktarux, Azimer).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

/**
 * \brief Reads a file into a buffer
 * \param path The file's path
 * \return The file's contents, or an empty vector if the operation failed
 */
std::vector<uint8_t> read_file_buffer(const std::filesystem::path& path);

/**
 * \brief Reads source data into the destination, advancing the source pointer by <c>len</c>
 * \param src A pointer to the source data
 * \param dest The destination buffer
 * \param len The destination buffer's length
 */
void memread(uint8_t** src, void* dest, unsigned int len);

/**
 * Converts a string to a wstring. Returns an empty string on fail.
 * \param str The string to convert
 * \return The string's wstring representation
 */
std::wstring string_to_wstring(const std::string& str);

/**
 * Converts a wstring to a string.
 * \param wstr The wstring to convert
 * \return The wstring's string representation
 */
std::string wstring_to_string(const std::wstring& wstr);
