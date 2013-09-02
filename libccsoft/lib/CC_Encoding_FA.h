/*
 Copyright 2013 Edouard Griffiths <f4exb at free dot fr>

 This file is part of CCSoft. A Convolutional Codes Soft Decoding library

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Boston, MA  02110-1301  USA

 Convolutional encoder class.

 This version uses a fixed array for internal registers

 */
#ifndef __CC_ENCODING_FA_H__
#define __CC_ENCODING_FA_H__

#include "CCSoft_Exception.h"
#include <vector>
#include <iostream>
#include <array>
#include <algorithm>

namespace ccsoft
{

/**
 * Print the content of a register in hexadecimal to an output stream
 * \tparam T_Register Type of register
 * \param reg Register
 * \param os Output stream
 */
template<typename T_Register>
void print_register(const T_Register& reg, std::ostream& os)
{
    os << std::hex << reg;
    os << std::dec;
}

/**
 * Print the content of a unsigned char register in hexadecimal to an output stream
 * \param reg Register
 * \param os Output stream
 */
template<>
void print_register<unsigned char>(const unsigned char& reg, std::ostream& os);

/**
 * Print the content of a I/O symbol to an output stream
 * \tparam T_IOSymbol Type of I/O symbol
 * \param sym I/O symbol
 * \param os Output stream
 */
template<typename T_IOSymbol>
void print_symbol(const T_IOSymbol& sym, std::ostream& os)
{
    os << std::dec << sym;
}

/**
 * Print the content of a unsigned char I/O symbol yo an output stream
 * \param sym I/O symbol
 * \param os Output stream
 */
template<>
void print_symbol<unsigned char>(const unsigned char& sym, std::ostream& os);

/**
 * \brief Convolutional encoding class. Supports any k,n with k<n.
 * The input bits of a symbol are clocked simultaneously into the right hand side, or least significant position of the internal
 * registers. Therefore the given polynomial representation of generators should follow the same convention.
 * This version uses a fixed array to store registers. The size is given by the N_k template parameter.
 * \tparam T_Register type of the internal registers
 * \tparam T_IOSymbol type used to pass input and output symbols
 * \tparam N_k Size of an input symbol in bits (k parameter)
 */
template<typename T_Register, typename T_IOSymbol, unsigned int N_k>
class CC_Encoding_FA
{
public:

    //=============================================================================================
    /**
     * Constructor.
     * \param _constraints Vector of register lengths (constraint length + 1). The number of elements determines k.
     * \param _genpoly_representations Generator polynomial numeric representations. There are as many elements as there
     * are input bits (k). Each element is itself a vector with one polynomial value per output bit. The smallest size of
     * these vectors is retained as the number of output bits n. The input bits of a symbol are clocked simultaneously into
     * the right hand side, or least significant position of the internal registers. Therefore the given polynomial representation
     * of generators should follow the same convention.
     */
    CC_Encoding_FA(const std::vector<unsigned int>& _constraints, const std::vector<std::vector<T_Register> >& _genpoly_representations) :
        k(N_k),
        constraints(_constraints),
        genpoly_representations(_genpoly_representations),
        m(0)
    {
        if (_constraints.size() !=  N_k)
        {
            throw CCSoft_Exception("Constraints size is not valid");
        }

        if (N_k > sizeof(T_IOSymbol)*8)
        {
            throw CCSoft_Exception("Number of input bits not supported by I/O symbol type");
        }

        if (genpoly_representations.size() != N_k)
        {
            throw CCSoft_Exception("Generator polynomial representations size error");
        }

        clear();
        unsigned int min_nb_outputs = genpoly_representations[0].size();

        for (unsigned int ci=0; ci < constraints.size(); ci++)
        {
            if (constraints[ci] > sizeof(T_Register)*8)
            {
                throw CCSoft_Exception("One constraint size is too large for the size of the registers");
            }

            if (genpoly_representations[ci].size() < min_nb_outputs)
            {
                min_nb_outputs = genpoly_representations[ci].size();
            }

            if (constraints[ci] > m)
            {
                m = constraints[ci];
            }
        }

        n = min_nb_outputs;

        if (n <= k)
        {
            throw CCSoft_Exception("The number of outputs must be larger than the number of inputs");
        }

        if (n > sizeof(T_IOSymbol)*8)
        {
            throw CCSoft_Exception("Number of output bits not supported by I/O symbol type");
        }
    }

    //=============================================================================================
    /**
     * Destructor
     */
    ~CC_Encoding_FA()
    {}

    //=============================================================================================
    /**
     * Clear internal registers. Used before encoding a sequence
     */
    void clear()
    {
        std::fill(registers.begin(), registers.end(), 0);
        //registers.fill(0);
    }

    //=============================================================================================
    /**
     * Encode a new symbol of k bits into a symbol of n bits
     * \param in_symbol Input symbol
     * \param out_symbol Output symbol
     * \param no_step Do not step registers before insert (used for assumptions during decoding)
     * \return true if successful
     */
    bool encode(const T_IOSymbol& in_symbol, T_IOSymbol& out_symbol, bool no_step = false)
    {
        T_IOSymbol w_in = in_symbol;

        // load registers with new symbol bits
        for (unsigned int ki=0; ki<k; ki++)
        {
            if (no_step)
            {
                registers[ki] >>= 1; // flush bit
            }
            registers[ki] <<= 1; // make room for bit
            registers[ki] += w_in & 1; // insert bit
            w_in >>= 1; // move to next input bit
        }

        out_symbol = 0;
        T_IOSymbol symbol_bit;

        for (unsigned int ni=0; ni<n; ni++)
        {
            symbol_bit = 0;

            for (unsigned int ki=0; ki<k; ki++)
            {
                symbol_bit ^= (xorbits(registers[ki]&genpoly_representations[ki][ni]) ? 1 : 0);
            }

            out_symbol += symbol_bit << ni;
        }

        return true;
    }

    //=============================================================================================
    /**
     * Prints encoding characteristics to an output stream
     * \param os The output stream
     */
    void print(std::ostream& os)
    {
        std::cout << "k=" << k << ", n=" << n << ", m=" << m << std::endl;

        for (unsigned int ci=0; ci<k; ci++)
        {
            os << ci << " (" << constraints[ci] << ") : ";

            for (unsigned int gi=0; gi<n; gi++)
            {
                print_register(genpoly_representations[ci][gi], os);
                os << " ";
            }

            os << std::endl;
        }
    }

    /**
     * Get the k parameter
     */
    unsigned int get_k() const
    {
        return k;
    }

    /**
     * Get the n paramater
     */
    unsigned int get_n() const
    {
        return n;
    }

    /**
     * Get the maximum register size
     */
    unsigned int get_m() const
    {
        return m;
    }

    /**
     * Get registers reference
     */
    const std::array<T_Register, N_k>& get_registers() const
    {
        return registers;
    }

    /**
     * Set registers
     */
    void set_registers(const std::array<T_Register, N_k>& _registers)
    {
        registers = _registers;
    }


protected:

    /**
     * XOR all bits in a register. Uses the bit counting method.
     * \tparam T_Register Type of register
     * \param reg Register
     * \return true=1 or false=0
     */
    bool xorbits(const T_Register& reg)
    {
        T_Register w_reg = reg;
        unsigned int nb_ones = 0;

        while(w_reg != 0)
        {
            nb_ones += w_reg % 2;
            w_reg /= 2;
        }

        return (nb_ones % 2) == 1;
    }

    unsigned int k; //!< Number of input bits or input symbol size in bits
    unsigned int n; //!< Number of output bits or output symbol size in bits
    unsigned int m; //!< Maximum register length
    std::vector<unsigned int> constraints; //!< As many constraints as there are inputs
    std::vector<std::vector<T_Register> > genpoly_representations; //!< As many generator polynomials vectors (the size of the number of outputs) as there are inputs
    std::array<T_Register, N_k> registers; //!< Memory registers as many as there are inputs
};

} // namespace ccsoft


#endif // __CC_ENCODING_FA_H__
