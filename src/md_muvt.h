#ifndef __MAPP__md_muvt__
#define __MAPP__md_muvt__
#include "md_nvt.h"
namespace MAPP_NS
{
    class MDMuVT:public MDNVT
    {
    private:
        int seed;
        type0 mu;
        std::string gas_elem_name;
        int nevery;
        int nattempts;
	
	type0 xlo;
	type0 xhi;
	type0 ylo;
	type0 yhi;
	type0 zlo;
	type0 zhi;
    protected:
        void update_x_d__x(type0);
        void update_x_d__x_w_dof(type0);
        void update_x_d();
        void update_x_d_w_dof();
        void pre_run_chk(AtomsMD*,ForceFieldMD*);
        void pre_init();
    public:
//	
        MDMuVT(type0,type0,type0,std::string,int, type0,type0,type0,type0,type0,type0);
        ~MDMuVT();
        void init();
        void run(int);
        void fin();
        
        typedef struct
        {
            PyObject_HEAD
            MDMuVT* md;
            ExportMD::Object* xprt;
        }Object;
        
        static PyTypeObject TypeObject;
        static PyObject* __new__(PyTypeObject*,PyObject*, PyObject*);
        static int __init__(PyObject*, PyObject*,PyObject*);
        static PyObject* __alloc__(PyTypeObject*,Py_ssize_t);
        static void __dealloc__(PyObject*);
        
        
        static PyMethodDef methods[];
        static void setup_tp_methods();
        static void ml_run(PyMethodDef&);
        
        
        static PyGetSetDef getset[];
        static void setup_tp_getset();
        static void getset_nevery(PyGetSetDef&);
        static void getset_nattempts(PyGetSetDef&);
        static void getset_seed(PyGetSetDef&);
        static void getset_gas_element(PyGetSetDef&);
        
	static void getset_xlo(PyGetSetDef&);
        static void getset_xhi(PyGetSetDef&);
        static void getset_ylo(PyGetSetDef&);
        static void getset_yhi(PyGetSetDef&);
        static void getset_zlo(PyGetSetDef&);
        static void getset_zhi(PyGetSetDef&);
        
        
        static int setup_tp();
        
    };
}
#endif
