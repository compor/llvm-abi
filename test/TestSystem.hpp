#ifndef TESTSYSTEM_HPP
#define TESTSYSTEM_HPP

#include <fstream>
#include <memory>
#include <stdexcept>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_os_ostream.h>

#include <llvm-abi/ABI.hpp>
#include <llvm-abi/Builder.hpp>
#include <llvm-abi/FunctionEncoder.hpp>
#include <llvm-abi/FunctionType.hpp>
#include <llvm-abi/Type.hpp>
#include <llvm-abi/TypeBuilder.hpp>

using namespace llvm_abi;

static const char* const X86_64_TRIPLE = "x86_64-none-linux-gnu";

class TestBuilder: public Builder {
public:
	TestBuilder(llvm::Function& function)
	: function_(function),
	builder_(&(function.getEntryBlock())) { }
	
	IRBuilder& getEntryBuilder() {
		if (!function_.getEntryBlock().empty()) {
			builder_.SetInsertPoint(function_.getEntryBlock().begin());
		}
		return builder_;
	}
	
	IRBuilder& getBuilder() {
		builder_.SetInsertPoint(&(function_.getEntryBlock()));
		return builder_;
	}
	
private:
	llvm::Function& function_;
	IRBuilder builder_;
	
};

class TestSystem {
public:
	TestSystem(const std::string& triple)
	: context_(),
	module_("", context_),
	abi_(createABI(module_, llvm::Triple(triple))),
	nextIntegerValue_(1) { }
	
	ABI& abi() {
		return *abi_;
	}
	
	llvm::Constant* getConstantValue(const Type type) {
		switch (type.kind()) {
			case VoidType:
				return llvm::UndefValue::get(llvm::Type::getVoidTy(context_));
			case PointerType:
				return llvm::ConstantPointerNull::get(llvm::Type::getInt8PtrTy(context_));
			case UnspecifiedWidthIntegerType:
			case FixedWidthIntegerType:
				return llvm::ConstantInt::get(abi_->getLLVMType(type),
				                              nextIntegerValue_++);
			case FloatingPointType:
				return llvm::ConstantFP::get(abi_->getLLVMType(type),
				                             static_cast<double>(nextIntegerValue_++));
			case ComplexType:
				llvm_unreachable("TODO");
			case StructType: {
				llvm::SmallVector<llvm::Type*, 8> types;
				llvm::SmallVector<llvm::Constant*, 8> values;
				for (const auto& structMember: type.structMembers()) {
					types.push_back(abi_->getLLVMType(structMember.type()));
					values.push_back(getConstantValue(structMember.type()));
				}
				
				const auto structType = llvm::StructType::get(context_, types);
				return llvm::ConstantStruct::get(structType, values);
			}
			case ArrayType: {
				llvm::SmallVector<llvm::Constant*, 8> values;
				for (size_t i = 0; i < type.arrayElementCount(); i++) {
					values.push_back(getConstantValue(type.arrayElementType()));
				}
				
				const auto arrayType = llvm::ArrayType::get(abi_->getLLVMType(type.arrayElementType()),
				                                            type.arrayElementCount());
				return llvm::ConstantArray::get(arrayType, values);
			}
			case VectorType: {
				llvm::SmallVector<llvm::Constant*, 8> values;
				for (size_t i = 0; i < type.vectorElementCount(); i++) {
					values.push_back(getConstantValue(type.vectorElementType()));
				}
				
				return llvm::ConstantVector::get(values);
			}
		}
	}
	
	void doTest(const std::string& testName, const FunctionType& functionType) {
		const auto calleeFunction = llvm::cast<llvm::Function>(module_.getOrInsertFunction("callee", abi_->getFunctionType(functionType)));
		const auto callerFunction = llvm::cast<llvm::Function>(module_.getOrInsertFunction("caller", abi_->getFunctionType(functionType)));
		
		const auto attributes = abi_->getAttributes(functionType);
		calleeFunction->setAttributes(attributes);
		callerFunction->setAttributes(attributes);
		
		const auto entryBasicBlock = llvm::BasicBlock::Create(context_, "", callerFunction);
		(void) entryBasicBlock;
		
		TestBuilder builder(*callerFunction);
		
		llvm::SmallVector<llvm::Value*, 8> encodedArgumentValues;
		for (auto it = callerFunction->arg_begin();
		     it != callerFunction->arg_end(); ++it) {
			encodedArgumentValues.push_back(it);
		}
		
		auto functionEncoder = abi_->createFunction(builder,
		                                            functionType,
		                                            encodedArgumentValues);
		
		const auto returnValue = abi_->createCall(
			builder,
			functionType,
			[&](llvm::ArrayRef<llvm::Value*> values) -> llvm::Value* {
				const auto callInst = builder.getBuilder().CreateCall(calleeFunction, values);
				callInst->setAttributes(attributes);
				return callInst;
			},
			functionEncoder->arguments()
		);
		
		functionEncoder->returnValue(returnValue);
		
		std::string filename;
		filename += "test-";
		filename += abi_->name();
		filename += "-" + testName;
		filename += ".output.ll";
		
		std::ofstream file(filename.c_str());
		file << "; Generated from: " << std::endl;
		file << "; " << functionType.toString() << std::endl;
		
		llvm::raw_os_ostream ostream(file);
		ostream << module_;
	}
	
private:
	llvm::LLVMContext context_;
	llvm::Module module_;
	std::unique_ptr<ABI> abi_;
	uint64_t nextIntegerValue_;
	
};

#endif