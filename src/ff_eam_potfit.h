#ifndef __MAPP__ff_eam_potfit__
#define __MAPP__ff_eam_potfit__
#include "ff_md.h"
#include "xmath.h"
#include "potfit_funcs.h"
#include "atoms_md.h"
#include "neighbor_md.h"
#include "dynamic_md.h"
#include "memory.h"
namespace MAPP_NS
{
    class ForceFieldEAMPotFitAckOgata
    {
    private:
    protected:
    public:
        static void ml_new(PyMethodDef&);
    };
    
    template<size_t NELEMS>
    class ForceFieldEAMPotFit: public ForceFieldMD
    {
    private:
        size_t nvs;
        type0* vs;
        type0* dvs;
        type0* dvs_lcl;
        
        std::string names[NELEMS];
        PotFitPairFunc* rho_ptr[NELEMS][NELEMS];
        PotFitPairFunc* phi_ptr[NELEMS][NELEMS];
        PotFitEmbFunc* F_ptr[NELEMS];
        type0 phi_calc(elem_type,elem_type,type0);
        type0 rho_calc(elem_type,elem_type,type0);
        type0 F_calc(elem_type,type0);
        type0 fpair_calc(elem_type,elem_type,type0,type0,type0);
        void reorder(std::string*,size_t);
        
        Vec<type0>* rho_vec_ptr;
    protected:
        void force_calc();
        void energy_calc();
        void pre_xchng_energy(GCMC*){};
        type0 xchng_energy(GCMC*){return 0.0;};
        void post_xchng_energy(GCMC*){};
    public:
        ForceFieldEAMPotFit(AtomsMD*,
        std::string(&)[NELEMS],
        type0*&,size_t,
        PotFitPairFunc*(&)[NELEMS][NELEMS],PotFitPairFunc*(&)[NELEMS][NELEMS],PotFitEmbFunc*(&)[NELEMS]);
        virtual ~ForceFieldEAMPotFit();
        void set_cutoff();
        void init();
        void fin();
        void init_xchng(){};
        void fin_xchng(){};
        void energy_gradient();
        
    };
}
/*--------------------------------------------
 constructor
 --------------------------------------------*/
template<size_t NELEMS>
ForceFieldEAMPotFit<NELEMS>::ForceFieldEAMPotFit(AtomsMD* __atoms,
std::string (&__names)[NELEMS],
type0*& __vs,size_t __nvs,
PotFitPairFunc*(& __phi_ptr)[NELEMS][NELEMS],
PotFitPairFunc*(&__rho_ptr)[NELEMS][NELEMS],
PotFitEmbFunc*(&__F_ptr)[NELEMS]):
ForceFieldMD(__atoms),vs(__vs),nvs(__nvs)
{
    memcpy(&phi_ptr[0][0],&__phi_ptr[0][0],NELEMS*NELEMS*sizeof(PotFitPairFunc*));
    memcpy(&rho_ptr[0][0],&__rho_ptr[0][0],NELEMS*NELEMS*sizeof(PotFitPairFunc*));
    memcpy(F_ptr,__F_ptr,NELEMS*sizeof(PotFitEmbFunc*));
    for(size_t i=0;i<NELEMS;i++) names[i]=__names[i];
    reorder(__atoms->elements.names, __atoms->elements.nelems);
    set_cutoff();
    Memory::alloc(dvs,nvs);
    Memory::alloc(dvs_lcl,nvs);
    ptrdiff_t offset;
    for(size_t i=0;i<NELEMS;i++)
    {
        offset=F_ptr[i]->vars-vs;
        F_ptr[i]->dvars_lcl=dvs_lcl+offset;
        
        for(size_t j=0;j<NELEMS;j++)
        {
            offset=phi_ptr[i][j]->vars-vs;
            phi_ptr[i][j]->dvars_lcl=dvs_lcl+offset;
            offset=rho_ptr[i][j]->vars-vs;
            rho_ptr[i][j]->dvars_lcl=dvs_lcl+offset;
        }
    }
}
/*--------------------------------------------
 destructor
 --------------------------------------------*/
template<size_t NELEMS>
ForceFieldEAMPotFit<NELEMS>::~ForceFieldEAMPotFit()
{
    Memory::dealloc(dvs_lcl);
    Memory::dealloc(dvs);
    Memory::dealloc(vs);
    for(size_t i=0;i<NELEMS;i++)
    {
        for(size_t j=1;j<NELEMS;j++)
        {
            if(rho_ptr[i][j]!=rho_ptr[i][0]) delete rho_ptr[i][j];
            rho_ptr[i][j]=NULL;
        }
        delete rho_ptr[i][0];
        rho_ptr[i][0]=0;
        delete F_ptr[i];
        F_ptr[i]=NULL;
    }
    for(size_t i=0;i<NELEMS;i++)
        for(size_t j=0;j<i+1;j++)
        {
            delete phi_ptr[i][j];
            phi_ptr[i][j]=phi_ptr[j][i]=NULL;
        }
}
/*--------------------------------------------
 
 --------------------------------------------*/
template<size_t NELEMS>
void ForceFieldEAMPotFit<NELEMS>::set_cutoff()
{
    for(size_t i=0;i<nelems;i++)
        for(size_t j=0;j<i+1;j++)
        {
            cut[i][j]=cut[j][i]=MAX(phi_ptr[i][j]->rc,MAX(rho_ptr[i][j]->rc,rho_ptr[j][i]->rc));
            cut_sq[i][j]=cut_sq[j][i]=cut[i][j]*cut[i][j];
        }
}
/*--------------------------------------------
 
 --------------------------------------------*/
template<size_t NELEMS>
void ForceFieldEAMPotFit<NELEMS>::init()
{
    pre_init();
    rho_vec_ptr=new Vec<type0>(atoms,1,"rho");
}
/*--------------------------------------------
 
 --------------------------------------------*/
template<size_t NELEMS>
void ForceFieldEAMPotFit<NELEMS>::fin()
{
    delete rho_vec_ptr;
    post_fin();
}
/*--------------------------------------------
 force and energy calculation
 --------------------------------------------*/
template<size_t NELEMS>
void ForceFieldEAMPotFit<NELEMS>::force_calc()
{
    type0* xvec=atoms->x->begin();
    type0* fvec=f->begin();
    type0* rho=rho_vec_ptr->begin();
    elem_type* evec=atoms->elem->begin();
    
    int iatm,jatm;
    elem_type ielem,jelem;
    type0 r,rsq;
    type0 fpair;
    type0 dx_ij[__dim__];
    int** neighbor_list=neighbor->neighbor_list;
    int* neighbor_list_size=neighbor->neighbor_list_size;
    const int natms_lcl=atoms->natms_lcl;
    for(int i=0;i<natms_lcl;i++)
        rho[i]=0.0;
    
    for(iatm=0;iatm<natms_lcl;iatm++)
    {
        ielem=evec[iatm];
        for(int j=0;j<neighbor_list_size[iatm];j++)
        {
            jatm=neighbor_list[iatm][j];
            jelem=evec[jatm];
            rsq=Algebra::RSQ<__dim__>(xvec+iatm*__dim__,xvec+jatm*__dim__);
            if(rsq>=cut_sq[ielem][jelem]) continue;
            r=sqrt(rsq);
            

            rho[iatm]+=rho_calc(jelem,ielem,r);
            if(jatm<natms_lcl)
            {
                rho[jatm]+=rho_calc(ielem,jelem,r);
                __vec_lcl[0]+=phi_calc(ielem,jelem,r);
            }
            else
                __vec_lcl[0]+=0.5*phi_calc(ielem,jelem,r);
        }
        __vec_lcl[0]+=F_calc(ielem,rho[iatm]);
    }
    
    
    
    dynamic->update(rho_vec_ptr);

    for(iatm=0;iatm<natms_lcl;iatm++)
    {
        ielem=evec[iatm];

        for(int j=0;j<neighbor_list_size[iatm];j++)
        {
            jatm=neighbor_list[iatm][j];
            jelem=evec[jatm];

            rsq=Algebra::DX_RSQ<__dim__>(xvec+iatm*__dim__,xvec+jatm*__dim__,dx_ij);
            if(rsq>=cut_sq[ielem][jelem]) continue;
            
            r=sqrt(rsq);
            
            
            fpair=fpair_calc(ielem,jelem,rho[iatm],rho[jatm],r);
            
            if(fpair==0.0) continue;
            
            Algebra::V_add_x_mul_V<__dim__>(fpair,dx_ij,fvec+iatm*__dim__);
            if(jatm<natms_lcl)
                Algebra::V_add_x_mul_V<__dim__>(-fpair,dx_ij,fvec+jatm*__dim__);
            else
                fpair*=0.5;
            
            Algebra::DyadicV<__dim__>(-fpair,dx_ij,&__vec_lcl[1]);
        }
    }
    type0 f_sum_lcl[__dim__];
    Algebra::zero<__dim__>(f_sum_lcl);
    for(int i=0;i<natms_lcl;i++)
        Algebra::V_add<__dim__>(fvec+__dim__*i,f_sum_lcl);

    type0 f_corr[__dim__];
    MPI_Allreduce(f_sum_lcl,f_corr,__dim__,Vec<type0>::MPI_T,MPI_SUM,world);
    type0 a=-1.0/static_cast<type0>(atoms->natms);
    Algebra::Do<__dim__>::func([&a,&f_corr](int i){f_corr[i]*=a;});
    for(int i=0;i<natms_lcl;i++)
        Algebra::V_add<__dim__>(f_corr,fvec+__dim__*i);
}
/*--------------------------------------------
 energy calculation
 --------------------------------------------*/
template<size_t NELEMS>
void ForceFieldEAMPotFit<NELEMS>::energy_calc()
{
    type0* xvec=atoms->x->begin();
    type0* rho=rho_vec_ptr->begin();
    elem_type* evec=atoms->elem->begin();
    
    int iatm,jatm;
    elem_type ielem,jelem;
    type0 r,rsq;
    
    int** neighbor_list=neighbor->neighbor_list;
    int* neighbor_list_size=neighbor->neighbor_list_size;
    const int natms_lcl=atoms->natms_lcl;
    for(int i=0;i<natms_lcl;i++)
        rho[i]=0.0;
    
    for(iatm=0;iatm<natms_lcl;iatm++)
    {
        ielem=evec[iatm];
        for(int j=0;j<neighbor_list_size[iatm];j++)
        {
            jatm=neighbor_list[iatm][j];
            jelem=evec[jatm];
            rsq=Algebra::RSQ<__dim__>(xvec+iatm*__dim__,xvec+jatm*__dim__);
            if(rsq>=cut_sq[ielem][jelem]) continue;
            r=sqrt(rsq);
            
            
            rho[iatm]+=rho_calc(jelem,ielem,r);
            if(jatm<natms_lcl)
            {
                rho[jatm]+=rho_calc(ielem,jelem,r);
                __vec_lcl[0]+=phi_calc(ielem,jelem,r);
            }
            else
                __vec_lcl[0]+=0.5*phi_calc(ielem,jelem,r);
        }
        // add the embedded energy here
        __vec_lcl[0]+=F_calc(ielem,rho[iatm]);
    }
}
/*--------------------------------------------
 energy calculation
 --------------------------------------------*/
template<size_t NELEMS>
void ForceFieldEAMPotFit<NELEMS>::energy_gradient()
{
    for(size_t i=0;i<nvs;i++) dvs_lcl[i]=0.0;
    type0* xvec=atoms->x->begin();
    type0* rho=rho_vec_ptr->begin();
    elem_type* evec=atoms->elem->begin();
    
    int iatm,jatm;
    elem_type ielem,jelem;
    type0 r,rsq,coef;
    
    int** neighbor_list=neighbor->neighbor_list;
    int* neighbor_list_size=neighbor->neighbor_list_size;
    const int natms_lcl=atoms->natms_lcl;
    for(int i=0;i<natms_lcl;i++)
        rho[i]=0.0;
    
    for(iatm=0;iatm<natms_lcl;iatm++)
    {
        ielem=evec[iatm];
        for(int j=0;j<neighbor_list_size[iatm];j++)
        {
            jatm=neighbor_list[iatm][j];
            jelem=evec[jatm];
            rsq=Algebra::RSQ<__dim__>(xvec+iatm*__dim__,xvec+jatm*__dim__);
            if(rsq>=cut_sq[ielem][jelem]) continue;
            r=sqrt(rsq);
            coef=jatm<natms_lcl ? 1.0:0.5;
            phi_ptr[ielem][jelem]->DF(coef,r);
            
            rho[iatm]+=rho_calc(jelem,ielem,r);
            if(jatm<natms_lcl)
            {
                rho[jatm]+=rho_calc(ielem,jelem,r);
                __vec_lcl[0]+=phi_calc(ielem,jelem,r);
            }
            else
                __vec_lcl[0]+=0.5*phi_calc(ielem,jelem,r);
        }

        __vec_lcl[0]+=F_calc(ielem,rho[iatm]);
        F_ptr[ielem]->DF(rho[iatm]);
    }
    
    type0 dFj,dFi;
    
    for(iatm=0;iatm<natms_lcl;iatm++)
    {
        ielem=evec[iatm];
        dFi=F_ptr[ielem]->dF(rho[iatm]);
        for(int j=0;j<neighbor_list_size[iatm];j++)
        {
            jatm=neighbor_list[iatm][j];
            jelem=evec[jatm];
            rsq=Algebra::RSQ<__dim__>(xvec+iatm*__dim__,xvec+jatm*__dim__);
            if(rsq>=cut_sq[ielem][jelem]) continue;
            r=sqrt(rsq);
            
            rho_ptr[jelem][ielem]->DF(dFi,r);
            if(jatm<natms_lcl)
            {
                dFj=F_ptr[jelem]->dF(rho[jatm]);
                rho_ptr[ielem][jelem]->DF(dFj,r);
            }
        }
    }
    MPI_Allreduce(dvs_lcl,dvs,static_cast<int>(nvs),Vec<type0>::MPI_T,MPI_SUM,world);
    MPI_Allreduce(__vec_lcl,__vec,1,Vec<type0>::MPI_T,MPI_SUM,world);
}
/*--------------------------------------------
 
 --------------------------------------------*/
template<size_t NELEMS>
type0 ForceFieldEAMPotFit<NELEMS>::phi_calc(elem_type ielem,elem_type jelem,type0 r)
{
    return phi_ptr[ielem][jelem]->F(r);
}
/*--------------------------------------------
 
 --------------------------------------------*/
template<size_t NELEMS>
type0 ForceFieldEAMPotFit<NELEMS>::rho_calc(elem_type ielem,elem_type jelem,type0 r)
{
    return rho_ptr[ielem][jelem]->F(r);
}
/*--------------------------------------------
 
 --------------------------------------------*/
template<size_t NELEMS>
type0 ForceFieldEAMPotFit<NELEMS>::F_calc(elem_type ielem,type0 rho)
{
    return F_ptr[ielem]->F(rho);
    
}
/*--------------------------------------------
 
 --------------------------------------------*/
template<size_t NELEMS>
type0 ForceFieldEAMPotFit<NELEMS>::fpair_calc(elem_type ielem,elem_type jelem,type0 rho_i,type0 rho_j,type0 r)
{
    return -(phi_ptr[ielem][jelem]->dF(r)+
             F_ptr[ielem]->dF(rho_i)*rho_ptr[jelem][ielem]->dF(r)+
             F_ptr[jelem]->dF(rho_j)*rho_ptr[ielem][jelem]->dF(r))/r;
}
/*--------------------------------------------
 
 --------------------------------------------*/
template<size_t NELEMS>
void ForceFieldEAMPotFit<NELEMS>::reorder(std::string* __names,size_t __nelems)
{
    if(__nelems>NELEMS) throw 1;
    
    size_t old_2_new[NELEMS];
    for(size_t i=0;i<NELEMS;i++) old_2_new[i]=i;
    for(size_t i=0;i<__nelems;i++)
    {
        bool fnd=false;
        for(size_t j=i;j<NELEMS&&!fnd;j++)
            if(strcmp(__names[i].c_str(),names[old_2_new[j]].c_str())==0)
            {
                std::swap(old_2_new[i],old_2_new[j]);
                fnd=true;
            }
        if(!fnd)
            throw 2;
    }
    
    
    PotFitPairFunc* __rho_ptr[NELEMS][NELEMS];
    PotFitPairFunc* __phi_ptr[NELEMS][NELEMS];
    PotFitEmbFunc* __F_ptr[NELEMS];
    std::string ___names[NELEMS];
    for(size_t i=0;i<NELEMS;i++)
    {
        ___names[i]=names[i];
        __F_ptr[i]=F_ptr[i];
        for(size_t j=0;j<NELEMS;j++)
        {
            __phi_ptr[i][j]=phi_ptr[i][j];
            __rho_ptr[i][j]=rho_ptr[i][j];
        }
    }
    
    
    for(size_t i=0;i<NELEMS;i++)
    {
        names[i]=___names[old_2_new[i]];
        F_ptr[i]=__F_ptr[old_2_new[i]];
        for(size_t j=0;j<NELEMS;j++)
        {
            phi_ptr[i][j]=__phi_ptr[old_2_new[i]][old_2_new[j]];
            rho_ptr[i][j]=__rho_ptr[old_2_new[i]][old_2_new[j]];
        }
    }
    
}

#endif


