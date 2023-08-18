// Fill out your copyright notice in the Description page of Project Settings.


#include "Syncer.h"


void USyncer::inc()
{
	this->requestCount++;
}

void USyncer::incN(int32 n)
{
	this->requestCount += n;
}

void USyncer::dec()
{
	this->requestCount = FMath::Max(this->requestCount-1, 0);//clamp to stop having values < 0
	if (this->requestCount == 0)
	{
		FOnDone().Execute();//fire off the delegate so anyone bound will know that we are done!
	}
}

void USyncer::decN(int32 n)
{
	this->requestCount = FMath::Max(this->requestCount - n, 0);//clamp to stop having values < 0
	if (this->requestCount == 0)
	{
		FOnDone().Execute();//fire off the delegate so anyone bound will know that we are done!
	}
}
