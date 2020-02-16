//
// Created by michel on 15-02-20.
//
#include <iostream>
#include <tdap/AlignedFrame.hpp>


template<size_t CHANNELS> void printAlignedResults() {
  using Alignment = tdap::Alignment<double, CHANNELS>;
  using Frame = tdap::AlignedFrame<double, CHANNELS>;
  std::cout << "Alignment<double," << CHANNELS << ">{elements=" << Alignment::elements
            << "; bytes=" << Alignment::bytes << "}" << std::endl;
  std::cout << "AlignedFrame<double," << CHANNELS << ">{channels=" << Frame::CHANNELS
            << "; bytes=" << Frame::ALIGN_BYTES
            << "; framesize=" << Frame::FRAMESIZE << "}" << std::endl;

}

void alignedFrameTests() {
  printAlignedResults<4>();
  printAlignedResults<5>();
  printAlignedResults<6>();
  printAlignedResults<7>();
  printAlignedResults<8>();
}

void testAdditionAndMultplication() {}
