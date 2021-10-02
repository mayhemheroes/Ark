/**
 * @file Compiler.hpp
 * @author Alexandre Plateau (lexplt.dev@gmail.com)
 * @brief ArkScript compiler is in charge of transforming the AST into bytecode
 * @version 0.3
 * @date 2020-10-27
 * 
 * @copyright Copyright (c) 2020-2021
 * 
 */

#ifndef ARK_COMPILER_COMPILER_HPP
#define ARK_COMPILER_COMPILER_HPP

#include <vector>
#include <string>
#include <cinttypes>
#include <optional>

#include <Ark/Platform.hpp>
#include <Ark/Compiler/Instructions.hpp>
#include <Ark/Compiler/AST/Node.hpp>
#include <Ark/Compiler/AST/Parser.hpp>
#include <Ark/Compiler/AST/Optimizer.hpp>
#include <Ark/Compiler/ValTableElem.hpp>

namespace Ark
{
    class State;

    /**
     * @brief The ArkScript bytecode compiler
     * 
     */
    class ARK_API Compiler
    {
    public:
        /**
         * @brief Construct a new Compiler object
         * 
         * @param debug the debug level
         * @param lib_dir the path to the standard library
         * @param options the compilers options
         */
        Compiler(unsigned debug, const std::string& lib_dir, uint16_t options = DefaultFeatures);

        /**
         * @brief Feed the differents variables with information taken from the given source code file
         * 
         * @param code the code of the file
         * @param filename the name of the file
         */
        void feed(const std::string& code, const std::string& filename = ARK_NO_NAME_FILE);

        /**
         * @brief Start the compilation
         * 
         */
        void compile();

        /**
         * @brief Save generated bytecode to a file
         * 
         * @param file the name of the file where the bytecode will be saved
         */
        void saveTo(const std::string& file);

        /**
         * @brief Return the constructed bytecode object
         * 
         * @return const bytecode_t& 
         */
        const bytecode_t& bytecode() noexcept;

        friend class Ark::State;

    private:
        internal::Parser m_parser;
        internal::Optimizer m_optimizer;
        uint16_t m_options;
        // tables: symbols, values, plugins and codes
        std::vector<internal::Node> m_symbols;
        std::vector<std::string> m_defined_symbols;
        std::vector<std::string> m_plugins;
        std::vector<internal::ValTableElem> m_values;
        std::vector<std::vector<uint8_t>> m_code_pages;
        std::vector<std::vector<uint8_t>> m_temp_pages;  ///< we need temporary code pages for some compilations passes

        bytecode_t m_bytecode;
        unsigned m_debug;  ///< the debug level of the compiler

        /**
         * @brief Push the first headers of the bytecode file
         * 
         */
        void pushHeadersPhase1() noexcept;

        /**
         * @brief Push the other headers, after having compile the file
         * (it's needed because we read the symbol/value tables, populated by compilation)
         * 
         */
        void pushHeadersPhase2();

        /**
         * @brief helper functions to get a temp or finalized code page
         * 
         * @param i page index, if negative, refers to a temporary code page
         * @return std::vector<uint8_t>& 
         */
        inline std::vector<uint8_t>& page(int i) noexcept
        {
            if (i >= 0)
                return m_code_pages[i];
            return m_temp_pages[-i - 1];
        }

        /**
         * @brief helper functions to get a temp or finalized code page
         * 
         * @param i page index, if negative, refers to a temporary code page
         * @return std::vector<uint8_t>* 
         */
        inline std::vector<uint8_t>* page_ptr(int i) noexcept
        {
            if (i >= 0)
                return &m_code_pages[i];
            return &m_temp_pages[-i - 1];
        }

        /**
         * @brief Count the number of "valid" ark objects in a node
         * @details Isn't considered valid a GetField, because we use
         *          this function to count the number of arguments of function calls.
         * 
         * @param lst 
         * @return std::size_t 
         */
        std::size_t countArkObjects(const std::vector<internal::Node>& lst) noexcept;

        /**
         * @brief Checking if a symbol is an operator
         * 
         * @param name symbol name
         * @return std::optional<std::size_t> position in the operators' list
         */
        std::optional<std::size_t> isOperator(const std::string& name) noexcept;

        /**
         * @brief Checking if a symbol is a builtin
         * 
         * @param name symbol name
         * @return std::optional<std::size_t> position in the builtins' list
         */
        std::optional<std::size_t> isBuiltin(const std::string& name) noexcept;

        /**
         * @brief Check if a symbol needs to be compiled to a specific instruction
         * 
         * @param name 
         * @return std::optional<internal::Instruction> corresponding instruction if it exists
         */
        inline std::optional<internal::Instruction> isSpecific(const std::string& name) noexcept
        {
            if (name == "list")
                return internal::Instruction::LIST;
            else if (name == "append")
                return internal::Instruction::APPEND;
            else if (name == "concat")
                return internal::Instruction::CONCAT;
            else if (name == "append!")
                return internal::Instruction::APPEND_IN_PLACE;
            else if (name == "concat!")
                return internal::Instruction::CONCAT_IN_PLACE;
            else if (name == "pop")
                return internal::Instruction::POP_LIST;
            else if (name == "pop!")
                return internal::Instruction::POP_LIST_IN_PLACE;

            return {};
        }

        /**
         * @brief Compute specific instruction argument count
         * 
         * @param inst 
         * @param previous 
         * @param p 
         */
        void pushSpecificInstArgc(internal::Instruction inst, uint16_t previous, int p) noexcept;

        /**
         * @brief Checking if a symbol may be coming from a plugin
         * 
         * @param name symbol name
         * @return true the symbol may be from a plugin, loaded at runtime
         * @return false 
         */
        bool mayBeFromPlugin(const std::string& name) noexcept;

        /**
         * @brief Throw a nice error message
         * 
         * @param message 
         * @param node 
         */
        void throwCompilerError(const std::string& message, const internal::Node& node);

        /**
         * @brief Compile a single node recursively
         * 
         * @param x the internal::Node to compile
         * @param p the current page number we're on
         */
        void _compile(const internal::Node& x, int p);

        void compileSymbol(const internal::Node& x, int p);
        void compileSpecific(const internal::Node& c0, const internal::Node& x, int p);
        void compileIf(const internal::Node& x, int p);
        void compileFunction(const internal::Node& x, int p);
        void compileLetMut(internal::Keyword n, const internal::Node& x, int p);
        void compileWhile(const internal::Node& x, int p);
        void compileSet(const internal::Node& x, int p);
        void compileQuote(const internal::Node& x, int p);
        void compilePluginImport(const internal::Node& x, int p);
        void compileDel(const internal::Node& x, int p);
        void handleCalls(const internal::Node& x, int p);

        /**
         * @brief Put a value in the bytecode, handling the closures chains
         * 
         * @param x 
         * @param p 
         */
        void putValue(const internal::Node& x, int p);

        /**
         * @brief Register a given node in the symbol table
         * @details Can throw if the table is full
         * 
         * @param sym 
         * @return uint16_t 
         */
        uint16_t addSymbol(const internal::Node& sym);

        /**
         * @brief Register a given node in the value table
         * @details Can throw if the table is full
         * 
         * @param x 
         * @return uint16_t 
         */
        uint16_t addValue(const internal::Node& x);

        /**
         * @brief Register a page id (function reference) in the value table
         * @details Can throw if the table is full
         * 
         * @param page_id 
         * @param current A reference to the current node, for context
         * @return std::size_t 
         */
        uint16_t addValue(std::size_t page_id, const internal::Node& current);

        /**
         * @brief Register a symbol as defined, so that later we can throw errors on undefined symbols
         * 
         * @param sym 
         */
        void addDefinedSymbol(const std::string& sym);

        /**
         * @brief Checks for undefined symbols, not present in the defined symbols table
         * 
         */
        void checkForUndefinedSymbol();

        /**
         * @brief Push a number on stack (need 2 bytes)
         * 
         * @param n the number to push
         * @param page the page where it should land, nullptr for current page
         */
        void pushNumber(uint16_t n, std::vector<uint8_t>* page = nullptr) noexcept;
    };

#include "Compiler.inl"
}

#endif
