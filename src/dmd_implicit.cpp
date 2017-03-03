#include "dmd_implicit.h"
#include "atoms_dmd.h"
#include "ff_dmd.h"
#include "dynamic_dmd.h"
#include "gmres.h"
#include "memory.h"
using namespace MAPP_NS;
/*--------------------------------------------
 
 --------------------------------------------*/
DMDImplicit::DMDImplicit():
DMD(),
max_niters_nonlin(5),
max_niters_lin(5),
y_0(NULL),
c_0(NULL),
del_c(NULL),
F(NULL),
a(NULL),
F_ptr(NULL),
del_c_ptr(NULL),
gmres(NULL),
__gmres(NULL)
{
}
/*--------------------------------------------
 
 --------------------------------------------*/
DMDImplicit::~DMDImplicit()
{
    delete [] y_0;
    y_0=c_0=a=NULL;
    delete F_ptr;
    delete del_c_ptr;
}
/*--------------------------------------------
 
 --------------------------------------------*/
void DMDImplicit::init_static()
{
    DMD::init_static();
    Memory::alloc(y_0,3*ncs);
    c_0=y_0+ncs;
    a=y_0+2*ncs;
    del_c_ptr=new Vec<type0>(atoms,c_dim);
    del_c=del_c_ptr->begin();
    F_ptr=new Vec<type0>(atoms,c_dim);
    F=F_ptr->begin();
    gmres=new GMRES<type0,ForceFieldDMD>(atoms,max_niters_lin,c_dim,*ff);
    __gmres=new __GMRES(atoms,max_niters_lin,c_dim);
}
/*--------------------------------------------
 
 --------------------------------------------*/
void DMDImplicit::fin_static()
{
    delete __gmres;
    __gmres=NULL;
    delete gmres;
    gmres=NULL;
    delete F_ptr;
    F_ptr=NULL;
    F=NULL;
    delete del_c_ptr;
    del_c_ptr=NULL;
    del_c=NULL;
    Memory::dealloc(y_0);
    y_0=c_0=a=NULL;
    DMD::fin_static();
}
/*--------------------------------------------
 
 --------------------------------------------*/
inline type0 DMDImplicit::update_c()
{
    type0 tmp;
    type0 r,r_lcl=1.0;
    for(int i=0;i<ncs;i++)
    {
        if(c[i]>=0.0)
        {
            tmp=c[i]+r_lcl*del_c[i];
            if(tmp>1.0)
            {
                r_lcl=(1.0-c[i])/del_c[i];
                while(c[i]+r_lcl*del_c[i]>1.0)
                    r_lcl=nextafter(r_lcl,0.0);
            }
            if(tmp<0.0)
            {
                r_lcl=-c[i]/del_c[i];
                while(c[i]+r_lcl*del_c[i]<0.0)
                    r_lcl=nextafter(r_lcl,0.0);
            }
        }
        else
            del_c[i]=0.0;
    }
    
    
    MPI_Allreduce(&r_lcl,&r,1,MPI_TYPE0,MPI_MIN,atoms->world);
    
    volatile type0 c0;
    for(int i=0;i<ncs;i++)
    {
        c0=c[i]+r*del_c[i];
        --++c0;
        c[i]=c0;
    }
    
    return r;
}
/*--------------------------------------------
 solve the implicit equation
 
 nc_dofs
 err_fac
 --------------------------------------------*/
bool DMDImplicit::nonlin()
{
    type0 res_tol=0.005*a_tol*sqrt(nc_dofs)/err_fac;
    type0 denom=0.1*a_tol*sqrt(nc_dofs)/err_fac;
    type0 norm=1.0,delta=0.0,delta_prev=0.0,ratio=1.0,R=1.0;
    
    int iter=0;

    
    
    
    memcpy(c_0,c,ncs*sizeof(type0));
    memcpy(c,y_0,ncs*sizeof(type0));
    dynamic->update(atoms->c);
    type0 cost=ff->update_J(beta,a,F)/(a_tol*sqrt(nc_dofs));
    //printf("------------------------------------------------\n");
    //printf("THE COST %d %e\n",0,cost);
    
    bool converge=false,diverge=false;
    while(iter<max_niters_nonlin && !converge && !diverge)
    {
        //gmres->solve(F_ptr,res_tol,norm,del_c_ptr);
        __gmres->solve(*ff,F_ptr,res_tol,norm,del_c_ptr);

        
        
        
        ratio=update_c();
        dynamic->update(atoms->c);
        
        delta=fabs(ratio*norm/denom);
        
        if(iter) R=MAX(0.3*R,delta/delta_prev);
        if(iter>1 && R*delta<1.0)
        {
            converge=true;
            continue;
        }
        
        if(iter>1 && delta/delta_prev>2.0)
        {
            diverge=true;
            continue;
        }

        delta_prev=delta;
        
        cost=ff->update_J(beta,a,F)/(a_tol*sqrt(nc_dofs));
        //printf("THE COST %d %e\n",iter+1,cost);
        iter++;
    }

    
    if(converge)
    {
        //printf("converged %e\n",dt);
        if(iter) nnonlin_acc++;
        return true;
    }
    //printf("diverged %e\n",dt);
    memcpy(c,c_0,ncs*sizeof(type0));
    dynamic->update(atoms->c);
    nnonlin_rej++;
    return false;
}
/*--------------------------------------------
 
 --------------------------------------------*/
#include <iostream>
#include "random.h"
#include "ff_eam_dmd.h"
void DMDImplicit::J_test()
{
    
    
    Random rand(234464565);
    constexpr int nvecs=100;
    type0 delta=1.0e-9;
    

    Vec<type0>* h =new Vec<type0>(atoms,c_dim);
    Vec<type0>* Jh =new Vec<type0>(atoms,c_dim);
    type0** dFs;
    Memory::alloc(dFs,nvecs,ncs);
    

    type0* __h=h->begin();
    for(int i=0;i<ncs;i++)
    {
        c[i]=rand.uniform();
        if(rand.uniform()>0.5)
        {
            __h[i]=0.00001*rand.uniform()*((1.0-c[i])/(delta*nvecs));
        }
        else
        {
            __h[i]=-0.00001*rand.uniform()*(c[i]/(delta*nvecs));
        }
    }
    
    
    
    
    
    
    
    
    
    
    
    ForceFieldEAMDMD* __ff=dynamic_cast<ForceFieldEAMDMD*>(ff);
    
    
    
    
    dynamic->update(atoms->c);
    __ff->update_J(beta,a,F);
    __ff->operator()(h,Jh);
    
    
    memcpy(c_0,c,ncs*sizeof(type0));
    for(int ivec=0;ivec<nvecs;ivec++)
    {
        type0 __delta=delta*ivec;
        for(int i=0;i<ncs;i++)
            c[i]=c_0[i]+__delta*__h[i];
        dynamic->update(atoms->c);
        __ff->update_J(beta,a,dFs[ivec]);
        for(int i=0;i<ncs;i++)
            dFs[ivec][i]-=F[i];
        
    }
    
    type0* __Jh=Jh->begin();
    FILE* fp=fopen("/Users/sina/Desktop/data.txt","w");
    for(int i=0;i<ncs;i++)
        for(int ivec=0;ivec<nvecs;ivec++)
            fprintf(fp,"%e\t%e\t%e\n",ivec*delta,dFs[ivec][i],-__Jh[i]*ivec*delta);

    
    fclose(fp);
    
    
    

    Memory::dealloc(dFs);
    delete Jh;
    delete h;
    
    
    
}




















/*------------------------------------------------------------------------------------------------------------------------------------
 
 ------------------------------------------------------------------------------------------------------------------------------------*/
PyObject* DMDImplicit::__new__(PyTypeObject* type,PyObject* args,PyObject* kwds)
{
    Object* __self=reinterpret_cast<Object*>(type->tp_alloc(type,0));
    PyObject* self=reinterpret_cast<PyObject*>(__self);
    return self;
}
/*--------------------------------------------
 
 --------------------------------------------*/
int DMDImplicit::__init__(PyObject* self,PyObject* args,PyObject* kwds)
{
    FuncAPI<> f("__init__");
    
    
    if(f(args,kwds)==-1) return -1;
    Object* __self=reinterpret_cast<Object*>(self);
    __self->dmd=new DMDImplicit();
    return 0;
}
/*--------------------------------------------
 
 --------------------------------------------*/
PyObject* DMDImplicit::__alloc__(PyTypeObject* type,Py_ssize_t)
{
    Object* __self=new Object;
    __self->ob_type=type;
    __self->ob_refcnt=1;
    __self->dmd=NULL;
    return reinterpret_cast<PyObject*>(__self);
}
/*--------------------------------------------
 
 --------------------------------------------*/
void DMDImplicit::__dealloc__(PyObject* self)
{
    Object* __self=reinterpret_cast<Object*>(self);
    delete __self->dmd;
    __self->dmd=NULL;
    delete __self;
}
/*--------------------------------------------*/
PyMethodDef DMDImplicit::methods[]={[0 ... 0]={NULL}};
/*--------------------------------------------*/
void DMDImplicit::setup_tp_methods()
{
}
/*--------------------------------------------*/
PyTypeObject DMDImplicit::TypeObject={PyObject_HEAD_INIT(NULL)};
/*--------------------------------------------*/
void DMDImplicit::setup_tp()
{
    TypeObject.tp_name="mapp.dmd.dmd_implicit";
    TypeObject.tp_doc="chemical integration";
    
    TypeObject.tp_flags=Py_TPFLAGS_DEFAULT;
    TypeObject.tp_basicsize=sizeof(Object);
    
    TypeObject.tp_new=__new__;
    TypeObject.tp_init=__init__;
    TypeObject.tp_alloc=__alloc__;
    TypeObject.tp_dealloc=__dealloc__;
    setup_tp_methods();
    TypeObject.tp_methods=methods;
    setup_tp_getset();
    TypeObject.tp_getset=getset;
}
/*--------------------------------------------*/
PyGetSetDef DMDImplicit::getset[]={[0 ... 2]={NULL,NULL,NULL,NULL,NULL}};
/*--------------------------------------------*/
void DMDImplicit::setup_tp_getset()
{
    getset_max_niters_lin(getset[0]);
    getset_max_niters_nonlin(getset[1]);
}
/*--------------------------------------------
 
 --------------------------------------------*/
void DMDImplicit::getset_max_niters_lin(PyGetSetDef& getset)
{
    getset.name=(char*)"max_niters_lin";
    getset.doc=(char*)"maximum number of iterations of linear solver (GMRES)";
    getset.get=[](PyObject* self,void*)->PyObject*
    {
        return var<int>::build(reinterpret_cast<Object*>(self)->dmd->max_niters_lin,NULL);
    };
    getset.set=[](PyObject* self,PyObject* op,void*)->int
    {
        VarAPI<int> max_niters_lin("max_niters_lin");
        max_niters_lin.logics[0]=VLogics("gt",0);
        int ichk=max_niters_lin.set(op);
        if(ichk==-1) return -1;
        reinterpret_cast<Object*>(self)->dmd->max_niters_lin=max_niters_lin.val;
        return 0;
    };
}
/*--------------------------------------------
 
 --------------------------------------------*/
void DMDImplicit::getset_max_niters_nonlin(PyGetSetDef& getset)
{
    getset.name=(char*)"max_niters_nonlin";
    getset.doc=(char*)"maximum number of iterations of linear solver (GMRES)";
    getset.get=[](PyObject* self,void*)->PyObject*
    {
        return var<int>::build(reinterpret_cast<Object*>(self)->dmd->max_niters_nonlin,NULL);
    };
    getset.set=[](PyObject* self,PyObject* op,void*)->int
    {
        VarAPI<int> max_niters_nonlin("max_niters_nonlin");
        max_niters_nonlin.logics[0]=VLogics("gt",0);
        int ichk=max_niters_nonlin.set(op);
        if(ichk==-1) return -1;
        reinterpret_cast<Object*>(self)->dmd->max_niters_nonlin=max_niters_nonlin.val;
        return 0;
    };
}