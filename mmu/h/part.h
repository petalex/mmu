#ifndef PART_H
#define PART_H

typedef unsigned long ClusterNo;
const unsigned long ClusterSize = 1024;

class PartitionImpl;

class Partition {
public:
	Partition(const char *);

	virtual ClusterNo getNumOfClusters() const;

	virtual int readCluster(ClusterNo, char *buffer);
	virtual int writeCluster(ClusterNo, const char *buffer);

	virtual ~Partition();
private:
	PartitionImpl *myImpl;

};

#endif
