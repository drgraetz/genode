/*
 * \brief  Genode/Nova specific VirtualBox SUPLib supplements
 * \author Alexander Boettcher
 * \date   2013-11-18
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode's VirtualBox includes */
#include "vcpu.h"
#include "svm.h"

class Vcpu_handler_svm : public Vcpu_handler
{
	private:

		__attribute__((noreturn)) void _svm_default() { _default_handler(); }

		__attribute__((noreturn)) void _svm_vintr() {
			_irq_window(SVM_EXIT_VINTR);
		}

		__attribute__((noreturn)) void _svm_ioio()
		{
			using namespace Nova;
			using namespace Genode;

			Thread_base *myself = Thread_base::myself();
			Utcb *utcb = reinterpret_cast<Utcb *>(myself->utcb());

			if (utcb->qual[0] & 0x4) {
				unsigned ctrl0 = utcb->ctrl[0];

				PERR("invalid gueststate");

				utcb->ctrl[0] = ctrl0;
				utcb->ctrl[1] = 0;
				utcb->mtd = Mtd::CTRL;
				
				Nova::reply(_stack_reply);
			}

			_default_handler();
		}

		template <unsigned X>
		__attribute__((noreturn)) void _svm_npt()
		{
			using namespace Nova;
			using namespace Genode;

			Thread_base *myself = Thread_base::myself();
			Utcb *utcb = reinterpret_cast<Utcb *>(myself->utcb());

			_exc_memory<X>(myself, utcb, utcb->qual[0] & 1,
			               utcb->qual[1] & ~((1UL << 12) - 1));
		}

		__attribute__((noreturn)) void _svm_startup()
		{
			using namespace Nova;

			/* enable VM exits for CPUID */
			next_utcb.mtd     = Nova::Mtd::CTRL;
			next_utcb.ctrl[0] = SVM_CTRL1_INTERCEPT_CPUID;
			next_utcb.ctrl[1] = 0;

			void *exit_status = _start_routine(_arg);
			pthread_exit(exit_status);

			Nova::reply(nullptr);
		}

	public:

		Vcpu_handler_svm(size_t stack_size, const pthread_attr_t *attr,
	                     void *(*start_routine) (void *), void *arg)
		: Vcpu_handler(stack_size, attr, start_routine, arg) 
		{
			using namespace Nova;

			typedef Vcpu_handler_svm This;

			register_handler<RECALL, This,
				&This::_svm_default>(vcpu().exc_base(), Mtd(Mtd::ALL | Mtd::FPU));
			register_handler<SVM_EXIT_IOIO, This,
				&This::_svm_ioio> (vcpu().exc_base(), Mtd(Mtd::ALL | Mtd::FPU));
			register_handler<SVM_EXIT_VINTR, This,
				&This::_svm_vintr> (vcpu().exc_base(), Mtd(Mtd::ALL | Mtd::FPU));
			register_handler<SVM_EXIT_RDTSC, This,
				&This::_svm_default> (vcpu().exc_base(), Mtd(Mtd::ALL | Mtd::FPU));
			register_handler<SVM_EXIT_MSR, This,
				&This::_svm_default> (vcpu().exc_base(), Mtd(Mtd::ALL | Mtd::FPU));
			register_handler<SVM_NPT, This,
				&This::_svm_npt<SVM_NPT>>(vcpu().exc_base(), Mtd(Mtd::ALL | Mtd::FPU));
			register_handler<SVM_EXIT_HLT, This,
				&This::_svm_default>(vcpu().exc_base(), Mtd(Mtd::ALL | Mtd::FPU));
			register_handler<SVM_EXIT_CPUID, This,
				&This::_svm_default> (vcpu().exc_base(), Mtd(Mtd::ALL | Mtd::FPU));
			register_handler<VCPU_STARTUP, This,
				&This::_svm_startup>(vcpu().exc_base(), Mtd(Mtd::ALL | Mtd::FPU));

			start();
		}

		bool hw_save_state(Nova::Utcb * utcb, VM * pVM, PVMCPU pVCpu) {
			return svm_save_state(utcb, pVM, pVCpu);
		}

		bool hw_load_state(Nova::Utcb * utcb, VM * pVM, PVMCPU pVCpu) {
			return svm_load_state(utcb, pVM, pVCpu);
		}
};