#ifndef __EuclideanMembershipFunction_h
#define __EuclideanMembershipFunction_h

#include "LabelPerPointMembershipFunction.h"

template< class TVector >
class EuclideanMembershipFunction :
	public LabelPerPointMembershipFunction< TVector >
{
	public:
		/** Standard class typedefs */
		typedef EuclideanMembershipFunction Self;
		typedef LabelPerPointMembershipFunction< TVector >    Superclass;
		typedef itk::SmartPointer<Self>                   Pointer;
		typedef itk::SmartPointer<const Self>             ConstPointer;

		/** Strandard macros */
		itkTypeMacro(EuclideanMembershipFunction,
				MembershipFunctionBase);
		itkNewMacro(Self);

		/** Typedef alias for the measurement vectors */
		typedef TVector MeasurementVectorType;
		typedef TVector CentroidType;

		/** Typedef to represent the length of measurement vectors */
		typedef typename Superclass::MeasurementVectorSizeType    MeasurementVectorSizeType;

		void SetCentroid( const MeasurementVectorType* c)
		{
			this->m_Centroid = c; 
		}
		const MeasurementVectorType* GetCentroid() const {
			return this->m_Centroid; 
		}

		std::vector<const MeasurementVectorType*> GetChilds()
		{
			return this->childs;
		}
		double GetVariance(){ return this->m_Variance/this->childs.size();}

		/**
		 * Method to get probability of an instance. The return value is the
		 * value of the density function, not probability. */
		double Evaluate(const MeasurementVectorType *measurement) const;
		double Evaluate(const MeasurementVectorType *m1,const MeasurementVectorType *m2)const ;
		double Evaluate(const MeasurementVectorType &measurement) const{ std::cout << "not implemented " << std::endl;return -1;};

		void AddChild( const MeasurementVectorType* measurement) ;
		void RecalculateCentroid() ;
		void ClearChilds()
		{ 
			this->childs.clear();
			
			this->m_Variance=0;
		}
		void WithCosine(bool on)
		{
			this->m_withCosine=on;
		}

		int GetNumberOfChilds(){return this->childs.size();}
		void AddDirectionalNeighbors(vnl_matrix<int>* neighbors)
		{
			this->m_directionalNeighbors.push_back(neighbors);
		}
		void ClearDirectionalNeighbors()
		{
			this->m_Variance = 0;
			this->m_directionalNeighbors.clear();
		}
	protected:
		EuclideanMembershipFunction(void);
		virtual ~EuclideanMembershipFunction(void) {}
		void PrintSelf(std::ostream& os, itk::Indent indent) const;

	private:
		double m_Variance;
	        std::vector<vnl_matrix<int>*> m_directionalNeighbors;		
		const MeasurementVectorType *m_Centroid;
		std::vector<const MeasurementVectorType*> childs;
		bool m_withCosine;
};
#include "EuclideanMembershipFunction.txx"
#endif
