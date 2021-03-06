#ifndef __ALG_ITEM_BASED_KNN_H__
#define __ALG_ITEM_BASED_KNN_H__

#include "RecSysAlgorithm.h"

#include <string>


class AlgItemBasedKnn
      : public RecSysAlgorithm< boost::numeric::ublas::mapped_matrix<double, boost::numeric::ublas::column_major> >
{
public:

   AlgItemBasedKnn( DataReader& dreader,
                    int userpos = 0,
                    int itempos = 1,
                    int ratingpos = 2 );

   int train();

   int train( size_t k );

   void test( DataFrame& dataFrame );

   double predict( std::string userId, std::string itemId );

private:

   size_t m_knn;

   std::map<std::string, double> m_meanRatingByItem;

   SparseMatrix< boost::numeric::ublas::mapped_matrix<double, boost::numeric::ublas::row_major> > m_simMatrix;

};

#endif // __ALG_ITEM_BASED_KNN_H__

